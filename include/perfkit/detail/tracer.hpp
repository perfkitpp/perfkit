//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <any>
#include <atomic>
#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <spdlog/fmt/fmt.h>

#include "array_view.hxx"
#include "interactive_image.hpp"
#include "spinlock.hxx"

namespace perfkit {
class tracer_future_result;

using clock_type = std::chrono::steady_clock;
struct trace_variant_type : std::variant<clock_type::duration,
                                         int64_t,
                                         double,
                                         std::string,
                                         bool,
                                         trace::interactive_image,
                                         std::any> {
  using variant::variant;
};

class tracer {
 public:
  using clock_type   = perfkit::clock_type;
  using variant_type = trace_variant_type;

  struct trace {
    std::optional<clock_type::duration> as_timer() const noexcept {
      if (auto ptr = std::get_if<clock_type::duration>(&data)) { return *ptr; }
      return {};
    }

    void subscribe(bool enabled) noexcept {
      _is_subscribed->store(enabled, std::memory_order_relaxed);
    }

    bool subscribing() const noexcept { return _is_subscribed->load(std::memory_order_relaxed); }

    void dump_data(std::string&) const;

   public:
    std::string_view key;
    uint64_t hash;

    size_t fence = 0;
    int order    = 0;
    array_view<std::string_view> hierarchy;

    variant_type data;

   private:
    friend class tracer;
    std::atomic_bool* _is_subscribed = {};
  };

  using fetched_traces = std::vector<trace>;

  static_assert(std::is_nothrow_move_assignable_v<trace>);
  static_assert(std::is_nothrow_move_constructible_v<trace>);

 private:
  struct _entity_ty {
    trace body;
    std::string key_buffer;
    std::vector<std::string_view> hierarchy;
    std::atomic_bool is_subscribed;
  };

 public:
  struct proxy {
   public:
    proxy(proxy&& o) noexcept {
      *this = std::move(o);
    }

    proxy branch(std::string_view n) noexcept;
    proxy timer(std::string_view n) noexcept;

    template <size_t N_>
    proxy operator[](char const (&s)[N_]) noexcept { return branch(s); }
    proxy operator[](std::string_view n) noexcept { return branch(n); }

    ~proxy() noexcept;  // if no data specified, ...

    variant_type& data() noexcept;

    template <typename Ty_>
    Ty_& data_as() noexcept {
      if (!std::holds_alternative<Ty_>(data())) {
        data().emplace<Ty_>();
      }
      return std::get<Ty_>(data());
    }

    auto& string() noexcept { return data_as<std::string>(); }
    auto& any() noexcept { return data_as<std::any>(); }

    template <typename... Args_>
    void format(std::string_view fmt, Args_&&... args) {
      string() = fmt::format(fmt, std::forward<Args_>(args)...);
    }

    template <typename Other_>
    proxy& operator=(Other_&& oty) noexcept {
      using other_t = std::remove_const_t<std::remove_reference_t<Other_>>;

      if constexpr (std::is_same_v<other_t, bool>) {
        data() = oty;
      } else if constexpr (std::is_integral_v<other_t>) {
        data() = static_cast<int64_t>(std::forward<Other_>(oty));
      } else if constexpr (std::is_floating_point_v<other_t>) {
        data() = static_cast<double>(std::forward<Other_>(oty));
      } else if constexpr (std::is_convertible_v<other_t, std::string>) {
        string() = static_cast<std::string>(std::forward<Other_>(oty));
      } else if constexpr (std::is_same_v<other_t, std::string_view>) {
        string() = static_cast<std::string>(std::forward<Other_>(oty));
      } else if constexpr (std::is_same_v<other_t, clock_type::duration>) {
        data() = std::forward<Other_>(oty);
      } else {
        any() = std::forward<Other_>(oty);
      }
      return *this;
    }

    proxy& operator=(proxy&& other) noexcept {
      this->~proxy();
      _owner             = other._owner;
      _ref               = other._ref;
      _epoch_if_required = other._epoch_if_required;

      other._owner             = {};
      other._ref               = {};
      other._epoch_if_required = {};

      return *this;
    }

    void commit() noexcept {
      *this = {};
    }

    operator bool() const noexcept {
      return _ref->is_subscribed.load(std::memory_order_acq_rel);
    }

    bool is_valid() const noexcept { return !!_owner && !!_ref; }

   private:
    friend class tracer;
    proxy() noexcept {}

   private:
    tracer* _owner                            = nullptr;
    _entity_ty* _ref                          = nullptr;
    clock_type::time_point _epoch_if_required = {};
  };

  struct async_trace_result {
   public:
    std::pair<
            std::unique_lock<perfkit::spinlock>,
            fetched_traces*>
    acquire() const noexcept {
      return std::make_pair(std::unique_lock{*_mtx_access}, _data);
    }

    void copy_sorted(fetched_traces& out) const noexcept;

   private:
    friend class tracer;
    perfkit::spinlock* _mtx_access;
    fetched_traces* _data;
  };

  using future_result = tracer_future_result;

 public:
  /**
   * Registers new memory block to global storage.
   *
   * @param order All blocks will be occur by its specified order.
   * @param name
   */
  tracer(int order, std::string_view name) noexcept;
  ~tracer() noexcept;

  static array_view<tracer*> all() noexcept;

 public:
  /**
   * Fork new proxy.
   *
   * @details
   *    fork() will increase sequence number by 1,
   *
   * @return
   */
  proxy fork(std::string const& n);

  /**
   * Reserves for async data sort
   * @return empty optional when there's any already queued operation.
   */
  void async_fetch_request(tracer::future_result* out);

  auto name() const noexcept { return _name; }
  auto order() const noexcept { return _occurence_order; }

 private:
  uint64_t _hash_active(_entity_ty const* parent, std::string_view top);

  // Create new or find existing.
  tracer::_entity_ty* _fork_branch(_entity_ty const* parent, std::string_view name, bool initial_subscribe_state);

  static std::vector<tracer*>& _all() noexcept;

 private:
  // 고려사항
  // 1. consumer가 오랫동안 안 읽어갈수도 있음 -> 매번 메모리 비우고 할당하기 = 낭비
  //    (e.g. 1초에 1000번 iteration ... 10번만 read ... 너무 큰 낭비다)
  //    따라서 항상 데이터 할당은 기존 데이터에 덮는 식으로 이루어져야.
  // 2. 프록시 생성 시 문자열 할당 최소화되어야 함. (가능한 룩업 한 번으로 끝내기)
  // 3.
  struct _impl;
  std::unique_ptr<_impl> self;

  // 0. fork가 호출되면 시퀀스 번호가 1 증가
  // 1. 새로운 문자열로 프록시 최초 생성 시 고정 슬롯 할당.
  // 2. 프록시가 데이터 넣을 때마다(타이머는 소멸 시) 데이터 블록의 백 버퍼 맵에 이름-값 쌍 할당
  // 3. 컨슈머는 data_block의 데이터를 복사 및 컨슈머 내의 버퍼 맵에 머지.
  //    이 때 최신 시퀀스 넘버도 같이 받는다.
  std::map<std::size_t, _entity_ty> _table;
  size_t _fence_active = 0;  // active sequence number of back buffer.

  int _order_active = 0;  // temporary variable for single iteration

  mutable spinlock _sort_merge_lock;
  fetched_traces _local_reused_memory;

  int _occurence_order;
  std::string const _name;
};

/**
 * Sort messages by rule
 *
 * When compare two ...
 *
 * 1. One is subset of other (hierarchy contains other)
 *   1. Always superset is preceded.
 * 2. Two are equal
 *   1. Fence
 *   2. Order
 * 3. Two are different
 *   1. Order
 */
void sort_messages_by_rule(tracer::fetched_traces&) noexcept;

}  // namespace perfkit
