// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "perfkit/common/array_view.hxx"
#include "perfkit/common/event.hxx"
#include "perfkit/common/hasher.hxx"
#include "perfkit/common/spinlock.hxx"

namespace fmt {
}

namespace perfkit {
class tracer;

using clock_type         = std::chrono::steady_clock;
using trace_variant_type = std::variant<nullptr_t,
                                        clock_type::duration,
                                        int64_t,
                                        double,
                                        std::string,
                                        bool>;

using trace_key_t = basic_key<class tracer>;

namespace _trace {
struct trace
{
    std::optional<clock_type::duration> as_timer() const noexcept
    {
        if (auto ptr = std::get_if<clock_type::duration>(&data)) { return *ptr; }
        return {};
    }

    void subscribe(bool enabled) noexcept
    {
        _is_subscribed->store(enabled, std::memory_order_relaxed);
    }
    void fold(bool folded) noexcept
    {
        _is_folded->store(folded, std::memory_order_relaxed);
    }

    trace_key_t unique_id() const noexcept
    {
        return trace_key_t::create(_is_subscribed);
    }

    auto _bk_p_subscribed() const noexcept { return _is_subscribed; }
    auto _bk_p_folded() const noexcept { return _is_folded; }

    bool subscribing() const noexcept { return _is_subscribed->load(std::memory_order_relaxed); }
    bool folded() const noexcept { return _is_folded->load(std::memory_order_relaxed); }

    void dump_data(std::string&) const;

   public:
    std::string_view key;
    uint64_t hash;

    size_t fence        = 0;
    size_t unique_order = 0;
    int active_order    = 0;
    array_view<std::string_view> hierarchy;
    trace const* owner_node = nullptr;
    trace const* self_node  = nullptr;

    trace_variant_type data;

   private:
    friend class ::perfkit::tracer;
    std::atomic_bool* _is_subscribed = {};
    std::atomic_bool* _is_folded     = {};
};

struct _entity_ty
{
    trace body;
    std::string key_buffer;
    std::vector<std::string_view> hierarchy;

    std::atomic_bool is_subscribed{false};
    std::atomic_bool is_folded{false};
    _entity_ty const* parent = nullptr;
};
}  // namespace _trace

class tracer_proxy
{
   public:
    tracer_proxy(tracer_proxy&& o) noexcept : _owner(nullptr)
    {
        *this = std::move(o);
    }

    tracer_proxy branch(std::string_view n) noexcept;
    tracer_proxy timer(std::string_view n) noexcept;

    template <size_t N_>
    tracer_proxy operator[](char const (&s)[N_]) noexcept
    {
        return branch(s);
    }
    tracer_proxy operator[](std::string_view n) noexcept { return branch(n); }

    ~tracer_proxy() noexcept;  // if no data specified, ...

   private:
    trace_variant_type& _data() noexcept;

    template <typename Ty_>
    Ty_& _data_as() noexcept
    {
        if (!std::holds_alternative<Ty_>(_data()))
        {
            _data().emplace<Ty_>();
        }
        return std::get<Ty_>(_data());
    }

    auto& _string() noexcept { return _data_as<std::string>(); }

   public:
    template <typename Str_, typename... Args_>
    tracer_proxy& operator()(Str_&& fmt, Args_&&... args)
    {
        using namespace fmt;

        if (is_valid())
            _string() = format(std::forward<Str_>(fmt), std::forward<Args_>(args)...);

        return *this;
    }

    template <typename Other_,
              typename = std::enable_if_t<not std::is_convertible_v<Other_, tracer_proxy>>>
    tracer_proxy& operator=(Other_&& oty) noexcept
    {
        if (not is_valid())
            return *this;

        using other_t = std::remove_const_t<std::remove_reference_t<Other_>>;

        if constexpr (std::is_same_v<other_t, bool>)
        {
            _data() = oty;
        }
        else if constexpr (std::is_integral_v<other_t>)
        {
            _data() = static_cast<int64_t>(std::forward<Other_>(oty));
        }
        else if constexpr (std::is_floating_point_v<other_t>)
        {
            _data() = static_cast<double>(std::forward<Other_>(oty));
        }
        else if constexpr (std::is_convertible_v<other_t, std::string>)
        {
            _string() = static_cast<std::string>(std::forward<Other_>(oty));
        }
        else if constexpr (std::is_same_v<other_t, std::string_view>)
        {
            _string() = static_cast<std::string>(std::forward<Other_>(oty));
        }
        else if constexpr (std::is_same_v<other_t, clock_type::duration>)
        {
            _data() = std::forward<Other_>(oty);
        }
        return *this;
    }

    tracer_proxy& operator=(tracer_proxy&& other) noexcept
    {
        this->~tracer_proxy();
        _owner             = other._owner;
        _ref               = other._ref;
        _epoch_if_required = other._epoch_if_required;

        other._owner             = {};
        other._ref               = {};
        other._epoch_if_required = {};

        return *this;
    }

    tracer_proxy& switch_to_timer(std::string_view name);

    operator bool() const noexcept
    {
        return is_valid() && _ref->is_subscribed.load(std::memory_order_consume);
    }

    bool is_valid() const noexcept { return _owner && _ref; }

   public:
    static tracer_proxy create_default() noexcept { return tracer_proxy{}; }

   private:
    friend class tracer;
    tracer_proxy() noexcept { (void)0; };

   private:
    tracer* _owner                            = nullptr;
    _trace::_entity_ty* _ref                  = nullptr;
    clock_type::time_point _epoch_if_required = {};
};

class tracer : public std::enable_shared_from_this<tracer>
{
   public:
    using clock_type     = perfkit::clock_type;
    using variant_type   = trace_variant_type;
    using fetched_traces = std::vector<_trace::trace>;
    using _entity_ty     = _trace::_entity_ty;
    using trace          = _trace::trace;
    using proxy          = tracer_proxy;

    static_assert(std::is_nothrow_move_assignable_v<_trace::trace>);
    static_assert(std::is_nothrow_move_constructible_v<_trace::trace>);

   public:
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
    static auto create(std::string_view name) noexcept { return create(0, name); }
    static std::vector<std::shared_ptr<tracer>> all() noexcept;

   public:
    event<fetched_traces const&> on_fetch;

   public:
    /**
     * Fork new proxy.
     *
     * @details
     *    fork() will increase sequence number by 1,
     *
     * @param n
     *    Initial name of root trace. Only the first invocation has effect.
     * @param interval
     *    If specified, fork only occurs when every [interval]th invocation
     *
     * @return
     */
    tracer_proxy fork(std::string_view n = "all", size_t interval = 0);

    /**
     * Create new timer branch from the topmost trace stack
     *
     * @return
     */
    tracer_proxy timer(std::string_view name);

    /**
     * Create new branch from topmost trace stack
     * @return
     */
    tracer_proxy branch(std::string_view name);

    /**
     * Create branch with value
     */
    template <typename ValTy_>
    tracer_proxy branch(std::string_view name, ValTy_&& val)
    {
        auto br = branch(name);
        br      = std::forward<ValTy_>(val);
        return br;
    }

    /**
     * Reserves for async data sort
     */
    void request_fetch_data();

    auto& name() const noexcept { return _name; }
    auto order() const noexcept { return _occurrence_order; }

   private:
    uint64_t _hash_active(_trace::_entity_ty const* parent, std::string_view top);
    bool _deliver_previous_result();

    // Create new or find existing.
    _trace::_entity_ty* _fork_branch(_trace::_entity_ty const* parent, std::string_view name, bool initial_subscribe_state);
    static std::vector<std::weak_ptr<tracer>>& _all() noexcept;
    void _try_pop(_trace::_entity_ty const* body);

   private:
    friend class tracer_proxy;

    // 0. fork가 호출되면 시퀀스 번호가 1 증가
    // 1. 새로운 문자열로 프록시 최초 생성 시 고정 슬롯 할당.
    // 2. 프록시가 데이터 넣을 때마다(타이머는 소멸 시) 데이터 블록의 백 버퍼 맵에 이름-값 쌍 할당
    // 3. 컨슈머는 data_block의 데이터를 복사 및 컨슈머 내의 버퍼 맵에 머지.
    //    이 때 최신 시퀀스 넘버도 같이 받는다.
    std::unordered_map<uint64_t, _trace::_entity_ty> _table;
    size_t _fence_active     = 0;  // active sequence number of back buffer.
    size_t _fence_latest     = 0;
    size_t _interval_counter = 0;

    int _order_active = 0;  // temporary variable for single iteration
    std::atomic_bool _pending_fetch;
    fetched_traces _local_reused_memory;

    int _occurrence_order;
    std::string const _name;

    std::vector<_entity_ty const*> _stack;
    clock_type::time_point _last_fork;
    clock_type::time_point _birth = clock_type::now();

    std::thread::id _working_thread_id = {};
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
