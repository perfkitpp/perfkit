//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <any>
#include <atomic>
#include <chrono>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#define KANGSW_ARRAY_VIEW_NAMESPACE perfkit
#include "array_view.hxx"
#include "spinlock.hxx"

namespace perfkit {
class message_block {
 public:
  using clock_type = std::chrono::steady_clock;
  using variant_type = std::variant<clock_type::duration,
                                    int64_t,
                                    double,
                                    std::string,
                                    bool,
                                    std::any>;

  struct msg_type {
    std::optional<clock_type::duration> as_timer() const noexcept {
      if (auto ptr = std::get_if<clock_type::duration>(&data)) { return *ptr; }
      return {};
    }

    void subscribe(bool enabled) noexcept {
      _is_subscribed->store(enabled, std::memory_order_acq_rel);
    }

    bool subscribing() const noexcept { return _is_subscribed->load(std::memory_order_acq_rel); }

   public:
    std::string_view key;
    uint64_t hash;

    size_t fence = 0;
    int order = 0;
    array_view<std::string_view> hierarchy;

    variant_type data;

   private:
    friend class message_block;
    std::atomic_bool* _is_subscribed = {};
  };

  using fetched_messages = std::vector<msg_type>;

  static_assert(std::is_nothrow_move_assignable_v<msg_type>, "");
  static_assert(std::is_nothrow_move_constructible_v<msg_type>, "");

  struct msg_entity {
    msg_type body;
    std::string key_buffer;
    std::vector<std::string_view> hierarchy;
    std::vector<uint64_t> hierarchy_hash;
    std::atomic_bool is_subscribed;
  };

  struct proxy {
   public:
    proxy(proxy&& o) noexcept {
      *this = std::move(o);
    }

    proxy branch(std::string_view n) noexcept;
    proxy timer(std::string_view n) noexcept;

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

    template <typename Other_>
    proxy& operator=(Other_&& oty) noexcept {
      if constexpr (std::is_integral_v<Other_>) {
        data() = static_cast<int64_t>(oty);
      } else if constexpr (std::is_floating_point_v<Other_>) {
        data() = static_cast<double>(oty);
      } else if constexpr (std::is_convertible_v<Other_, std::string>) {
        string() = static_cast<std::string>(oty);
      } else if constexpr (std::is_same_v<Other_, bool>) {
        data() = oty;
      } else if constexpr (std::is_same_v<Other_, clock_type::duration>) {
        data() = oty;
      } else {
        any() = oty;
      }
      return *this;
    }

    proxy& operator=(proxy&& other) noexcept {
      std::swap(_owner, other._owner);
      std::swap(_ref, other._ref);
      std::swap(_epoch_if_required, other._epoch_if_required);
      return *this;
    }

    operator bool() const noexcept {
      return _ref->is_subscribed.load(std::memory_order_acq_rel);
    }

    bool is_valid() const noexcept { return !!_owner && !!_ref; }

   private:
    friend class message_block;
    proxy() noexcept {}

   private:
    message_block* _owner = nullptr;
    msg_entity* _ref = nullptr;
    clock_type::time_point _epoch_if_required = {};
  };

  struct msg_fetch_result {
   public:
    std::pair<
        std::unique_lock<perfkit::spinlock>,
        fetched_messages*>
    acquire() noexcept {
      return std::make_pair(std::unique_lock{*_mtx_access}, _data);
    }

    void copy_sorted(fetched_messages& out) noexcept;

   private:
    friend class message_block;
    perfkit::spinlock* _mtx_access;
    fetched_messages* _data;
  };

 public:
  /**
   * Registers new memory block to global storage.
   *
   * @param order All blocks will be occur by its specified order.
   * @param name
   */
  message_block(int order, std::string_view name) noexcept;

  static array_view<message_block*> all_blocks() noexcept;

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
  std::shared_future<msg_fetch_result> async_fetch_request();

  auto name() const noexcept { return _name; }
  auto order() const noexcept { return _occurence_order; }

 private:
  uint64_t _hash_active(msg_entity const* parent, std::string_view top);

  // Create new or find existing.
  message_block::msg_entity* _fork_branch(msg_entity const* parent, std::string_view name, bool initial_subscribe_state);

  static std::vector<message_block*>& _all() noexcept;

 private:
  // 고려사항
  // 1. consumer가 오랫동안 안 읽어갈수도 있음 -> 매번 메모리 비우고 할당하기 = 낭비
  //    (e.g. 1초에 1000번 iteration ... 10번만 read ... 너무 큰 낭비다)
  //    따라서 항상 데이터 할당은 기존 데이터에 덮는 식으로 이루어져야.
  // 2. 프록시 생성 시 문자열 할당 최소화되어야 함. (가능한 룩업 한 번으로 끝내기)
  // 3.

  // 0. fork가 호출되면 시퀀스 번호가 1 증가
  // 1. 새로운 문자열로 프록시 최초 생성 시 고정 슬롯 할당.
  // 2. 프록시가 데이터 넣을 때마다(타이머는 소멸 시) 데이터 블록의 백 버퍼 맵에 이름-값 쌍 할당
  // 3. 컨슈머는 data_block의 데이터를 복사 및 컨슈머 내의 버퍼 맵에 머지.
  //    이 때 최신 시퀀스 넘버도 같이 받는다.
  std::map<std::size_t, msg_entity> _table;
  size_t _fence_active = 0;  // active sequence number of back buffer.

  int _order_active = 0;  // temporary variable for single iteration

  mutable spinlock _sort_merge_lock;
  std::optional<std::promise<msg_fetch_result>> _msg_promise;
  std::shared_future<msg_fetch_result> _msg_future;
  fetched_messages _local_reused_memory;

  int _occurence_order;
  std::string_view const _name;
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
void sort_messages_by_rule(message_block::fetched_messages&) noexcept;

}  // namespace perfkit
