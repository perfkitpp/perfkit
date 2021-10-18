//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "perfkit/common/array_view.hxx"
#include "perfkit/common/spinlock.hxx"

namespace fmt {}

namespace perfkit {
class tracer_future_result;
class tracer;

using clock_type = std::chrono::steady_clock;
struct trace_variant_type
        : std::variant<
                  nullptr_t,
                  clock_type::duration,
                  int64_t,
                  double,
                  std::string,
                  bool> {
  using variant::variant;
};

namespace _trace {
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

  trace_variant_type data;

 private:
  friend class ::perfkit::tracer;
  std::atomic_bool* _is_subscribed = {};
};

struct _entity_ty {
  trace body;
  std::string key_buffer;
  std::vector<std::string_view> hierarchy;
  std::atomic_bool is_subscribed;
};
}  // namespace _trace

class tracer_proxy {
 public:
  tracer_proxy(tracer_proxy&& o) noexcept {
    *this = std::move(o);
  }

  tracer_proxy branch(std::string_view n) noexcept;
  tracer_proxy timer(std::string_view n) noexcept;

  template <size_t N_>
  tracer_proxy operator[](char const (&s)[N_]) noexcept { return branch(s); }
  tracer_proxy operator[](std::string_view n) noexcept { return branch(n); }

  ~tracer_proxy() noexcept;  // if no data specified, ...

 private:
  trace_variant_type& _data() noexcept;

  template <typename Ty_>
  Ty_& _data_as() noexcept {
    if (!std::holds_alternative<Ty_>(_data())) {
      _data().emplace<Ty_>();
    }
    return std::get<Ty_>(_data());
  }

  auto& _string() noexcept { return _data_as<std::string>(); }

 public:
  template <typename Str_, typename... Args_>
  tracer_proxy& operator()(Str_&& fmt, Args_&&... args) {
    using namespace fmt;

    if (is_valid())
      _string() = format(std::forward<Str_>(fmt), std::forward<Args_>(args)...);

    return *this;
  }

  template <typename Other_>
  tracer_proxy& operator=(Other_&& oty) noexcept {
    if (not is_valid())
      return *this;

    using other_t = std::remove_const_t<std::remove_reference_t<Other_>>;

    if constexpr (std::is_same_v<other_t, bool>) {
      _data() = oty;
    } else if constexpr (std::is_integral_v<other_t>) {
      _data() = static_cast<int64_t>(std::forward<Other_>(oty));
    } else if constexpr (std::is_floating_point_v<other_t>) {
      _data() = static_cast<double>(std::forward<Other_>(oty));
    } else if constexpr (std::is_convertible_v<other_t, std::string>) {
      _string() = static_cast<std::string>(std::forward<Other_>(oty));
    } else if constexpr (std::is_same_v<other_t, std::string_view>) {
      _string() = static_cast<std::string>(std::forward<Other_>(oty));
    } else if constexpr (std::is_same_v<other_t, clock_type::duration>) {
      _data() = std::forward<Other_>(oty);
    }
    return *this;
  }

  tracer_proxy& operator=(tracer_proxy&& other) noexcept {
    this->~tracer_proxy();
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
    return is_valid() && _ref->is_subscribed.load(std::memory_order_consume);
  }

  bool is_valid() const noexcept { return _owner && _ref; }

 private:
  friend class tracer;
  tracer_proxy() noexcept { (void)0; };

 private:
  tracer* _owner                            = nullptr;
  _trace::_entity_ty* _ref                  = nullptr;
  clock_type::time_point _epoch_if_required = {};
};

class tracer {
 public:
  using clock_type     = perfkit::clock_type;
  using variant_type   = trace_variant_type;
  using fetched_traces = std::vector<_trace::trace>;
  using _entity_ty     = _trace::_entity_ty;
  using trace          = _trace::trace;
  using proxy          = tracer_proxy;

  static_assert(std::is_nothrow_move_assignable_v<_trace::trace>);
  static_assert(std::is_nothrow_move_constructible_v<_trace::trace>);

 private:
 public:
  struct async_trace_result {
   public:
    std::pair<
            std::unique_lock<std::mutex>,
            std::shared_ptr<fetched_traces>>
    acquire() const noexcept {
      return std::make_pair(std::unique_lock{*_mtx_access}, _data);
    }

    void copy_sorted(fetched_traces& out) const noexcept;

   private:
    friend class tracer;
    std::shared_ptr<std::mutex> _mtx_access;
    std::shared_ptr<fetched_traces> _data;
  };

  using future_result = tracer_future_result;

  /**
   * Registers new memory block to global storage.
   *
   * @param order All blocks will be occur by its specified order.
   * @param name
   */
 private:
  tracer(int order, std::string_view name) noexcept;

 public:
  ~tracer() noexcept;

 public:
  static auto create(int order, std::string_view name) noexcept -> std::shared_ptr<tracer>;
  static std::vector<std::shared_ptr<tracer>> all() noexcept;

 public:
  /**
   * Fork new proxy.
   *
   * @details
   *    fork() will increase sequence number by 1,
   *
   * @return
   */
  tracer_proxy fork(std::string const& n, size_t interval = 0);

  /**
   * Reserves for async data sort
   * @return empty optional when there's any already queued operation.
   */
  void async_fetch_request(tracer::future_result* out);

  auto name() const noexcept { return _name; }
  auto order() const noexcept { return _occurence_order; }

 private:
  uint64_t _hash_active(_trace::_entity_ty const* parent, std::string_view top);
  bool _deliver_previous_result();

  // Create new or find existing.
  _trace::_entity_ty* _fork_branch(_trace::_entity_ty const* parent, std::string_view name, bool initial_subscribe_state);
  static std::vector<std::weak_ptr<tracer>>& _all() noexcept;

 private:
  friend class tracer_proxy;

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
  std::map<std::size_t, _trace::_entity_ty> _table;
  size_t _fence_active     = 0;  // active sequence number of back buffer.
  size_t _fence_latest     = 0;
  size_t _interval_counter = 0;

  int _order_active = 0;  // temporary variable for single iteration

  mutable std::mutex _sort_merge_lock;
  fetched_traces _local_reused_memory;

  int _occurence_order;
  std::string const _name;

  std::weak_ptr<tracer> _self_weak;
};

using tracer_ptr  = std::shared_ptr<tracer>;
using tracer_wptr = std::weak_ptr<tracer>;

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
