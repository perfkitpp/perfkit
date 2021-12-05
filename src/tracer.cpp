//
// Created by Seungwoo on 2021-08-25.
//
#include "perfkit/detail/tracer.hpp"

#include <future>
#include <mutex>
#include <variant>

#include <nlohmann/detail/conversions/from_json.hpp>
#include <perfkit/common/assert.hxx>
#include <perfkit/common/hasher.hxx>
#include <perfkit/detail/trace_future.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

using namespace std::literals;
using namespace perfkit;

struct tracer::_impl
{
    std::optional<std::promise<async_trace_result>> msg_promise;
    tracer_future_result msg_future;
};

tracer::_entity_ty* tracer::_fork_branch(
        _entity_ty const* parent, std::string_view name, bool initial_subscribe_state)
{
    auto hash = _hash_active(parent, name);

    auto [it, is_new] = _table.try_emplace(hash);
    auto& data        = it->second;

    if (is_new)
    {
        data.key_buffer          = std::string(name);
        data.body.hash           = hash;
        data.body.key            = data.key_buffer;
        data.body._is_subscribed = &data.is_subscribed;
        data.body._is_folded     = &data.is_folded;
        data.is_subscribed.store(initial_subscribe_state, std::memory_order_relaxed);
        parent && (data.hierarchy = parent->hierarchy, 0);  // only includes parent hierarchy.
        data.hierarchy.push_back(data.key_buffer);
        data.body.hierarchy    = data.hierarchy;
        data.body.unique_order = _table.size();
    }

    data.parent            = parent;
    data.body.fence        = _fence_active;
    data.body.active_order = _order_active++;
    _stack.push_back(&data);

    return &data;
}

uint64_t tracer::_hash_active(_entity_ty const* parent, std::string_view top)
{
    // --> 계층은 전역으로 관리되면 안 됨 ... 각각의 프록시가 관리해야함!!
    // Hierarchy 각각의 데이터 엔티티 기반으로 관리되게 ... _hierarchy_hash 관련 기능 싹 갈아엎기

    auto hash = hasher::FNV_OFFSET_BASE;
    if (parent)
    {
        hash = parent->body.hash;
    }

    for (auto c : top) { hash = hasher::fnv1a_byte(c, hash); }
    return hash;
}

tracer_proxy tracer::fork(std::string const& n, size_t interval)
{
    if (_fence_active > _fence_latest)  // only when update exist...
        std::unique_lock{_sort_merge_lock}, _deliver_previous_result();

    if (interval > 1 && ++_interval_counter < interval)
        return {};  // if fork interval is set ...

    // init new iteration
    ++_fence_active;
    _order_active = 0;
    _stack.clear();

    tracer_proxy prx;
    prx._owner             = this;
    prx._ref               = _fork_branch(nullptr, n, false);
    prx._epoch_if_required = clock_type::now();

    return prx;
}

bool tracer::_deliver_previous_result()
{  // perform queued sort-merge operation
    if (not self->msg_promise)
        return false;

    auto& promise = *this->self->msg_promise;

    // copies all messages and put them to cache buffer to prevent memory reallocation
    // if any entity is folded, skip all of its subtree
    this->_local_reused_memory.clear();
    for (auto& [hash, entity] : _table)
    {
        bool folded = false;
        for (auto parent = entity.parent; parent; parent = parent->parent)
            if (parent->is_folded)
            {
                folded = true;
                break;
            }

        if (folded)
            continue;

        this->_local_reused_memory.emplace_back(entity.body);
    }

    auto self_ptr = this->_self_weak.lock();

    async_trace_result rs = {};
    rs._mtx_access        = decltype(rs._mtx_access){self_ptr, &this->_sort_merge_lock};
    rs._data              = decltype(rs._data){self_ptr, &this->_local_reused_memory};
    promise.set_value(rs);

    this->self->msg_future  = {};
    this->self->msg_promise = {};

    this->_fence_latest = this->_fence_active;
    return true;
}

namespace {
struct message_block_sorter
{
    int n;
    friend bool operator<(std::weak_ptr<tracer> ptr, message_block_sorter s)
    {
        return s.n > ptr.lock()->order();
    }
};
}  // namespace

static auto lock_tracer_repo = [] {
    static std::mutex _lck;
    return std::unique_lock{_lck};
};

tracer::tracer(int order, std::string_view name) noexcept
        : self(new _impl), _occurrence_order(order), _name(name)
{
}

std::vector<std::weak_ptr<tracer>>& tracer::_all() noexcept
{
    static std::vector<std::weak_ptr<tracer>> inst;
    return inst;
}

std::vector<std::shared_ptr<tracer>> tracer::all() noexcept
{
    return lock_tracer_repo(),
           [] {
               std::vector<std::shared_ptr<tracer>> ret{};
               ret.reserve(_all().size());
               std::transform(
                       _all().begin(), _all().end(), back_inserter(ret),
                       [](auto&& p) { return p.lock(); });
               return ret;
           }();
}

void tracer::async_fetch_request(tracer::future_result* out)
{
    auto _lck = std::unique_lock{_sort_merge_lock};

    // if there's already queued merge operation, share future once more.
    if (self->msg_promise)
    {
        *out = self->msg_future;
        return;
    }

    // if not, queue new promise and make its future shared.
    self->msg_promise.emplace();
    self->msg_future = std::shared_future(self->msg_promise->get_future());
    *out             = self->msg_future;
}

auto perfkit::tracer::create(int order, std::string_view name) noexcept -> std::shared_ptr<tracer>
{
    auto _{lock_tracer_repo()};
    SPDLOG_DEBUG("creating tracer {}", name);
    std::shared_ptr<tracer> entity{
            new tracer{order, name}
    };
    entity->_self_weak = entity;

    auto it_insert = std::lower_bound(_all().begin(), _all().end(), message_block_sorter{order});
    _all().insert(it_insert, entity);
    return entity;
}

perfkit::tracer::~tracer() noexcept
{
    static auto equivalent = [](auto&& p1, auto&& p2) {
        return p1.lock() == p2.lock();
    };

    auto _{lock_tracer_repo()};
    SPDLOG_DEBUG("destroying tracer {}", _name);
    auto it = std::find_if(_all().begin(), _all().end(),
                           [&](auto&& wptr) { return equivalent(wptr, _self_weak); });
    if (it != _all().end())
    {
        _all().erase(it);
        SPDLOG_DEBUG("erasing tracer from all {}", _name);
    }
    else
    {
        SPDLOG_DEBUG("logic error: tracer invalid! {}", _name);
    }
}

void perfkit::tracer::_try_pop(_trace::_entity_ty const* body)
{
    assert_(not _stack.empty());

    size_t i = _stack.size();
    while (--i != ~size_t{} && _stack[i] != body)
        ;

    assert_(i != ~size_t{});
    _stack.erase(_stack.begin() + i);
}

tracer_proxy perfkit::tracer::timer(std::string_view name)
{
    auto px               = branch(name);
    px._epoch_if_required = clock_type::now();
    return px;
}

tracer_proxy perfkit::tracer::branch(std::string_view name)
{
    tracer_proxy px;
    px._owner = this;
    px._ref   = _fork_branch(_stack.back(), name, false);
    return px;
};

tracer::proxy tracer::proxy::branch(std::string_view n) noexcept
{
    if (not is_valid()) { return {}; }

    tracer_proxy px;
    px._owner = _owner;
    px._ref   = _owner->_fork_branch(_ref, n, false);
    return px;
}

tracer::proxy tracer::proxy::timer(std::string_view n) noexcept
{
    if (not is_valid()) { return {}; }

    tracer_proxy px;
    px._owner             = _owner;
    px._ref               = _owner->_fork_branch(_ref, n, false);
    px._epoch_if_required = clock_type::now();
    return px;
}

tracer_proxy::~tracer_proxy() noexcept
{
    if (!_owner) { return; }
    _owner->_try_pop(_ref);

    if (_epoch_if_required != clock_type::time_point{})
    {
        _data() = clock_type::now() - _epoch_if_required;
    }

    // clear to prevent logic error
    _owner = nullptr;
    _ref   = nullptr;
}

tracer::variant_type& tracer::proxy::_data() noexcept
{
    return _ref->body.data;
}

tracer_proxy& tracer_proxy::switch_to_timer(std::string_view name)
{
    auto owner  = _owner;
    auto parent = _ref->parent;
    *this       = {};

    _ref               = owner->_fork_branch(parent, name, false);
    _owner             = owner;
    _epoch_if_required = clock_type::now();

    return *this;
}

void tracer::async_trace_result::copy_sorted(fetched_traces& out) const noexcept
{
    {
        auto [lck, ptr] = acquire();
        out             = *ptr;
    }
    sort_messages_by_rule(out);
}

namespace {
enum class hierarchy_compare_result
{
    irrelevant,
    a_contains_b,
    b_contains_a,
    equal
};

auto compare_hierarchy(array_view<std::string_view> a, array_view<std::string_view> b)
{
    size_t num_equals = 0;

    for (size_t i = 0, n_max = std::min(a.size(), b.size()); i < n_max; ++i)
    {
        auto r_cmp = a[i].compare(b[i]);
        if (r_cmp != 0) { break; }
        ++num_equals;
    }

    if (a.size() == b.size())
    {
        return num_equals == a.size()
                     ? hierarchy_compare_result::equal
                     : hierarchy_compare_result::irrelevant;
    }
    if (num_equals != a.size() && num_equals != b.size())
    {
        return hierarchy_compare_result::irrelevant;
    }

    if (num_equals == a.size())
    {
        return hierarchy_compare_result::b_contains_a;
    }
    else
    {
        return hierarchy_compare_result::a_contains_b;
    }
}

}  // namespace

void perfkit::sort_messages_by_rule(tracer::fetched_traces& msg) noexcept
{
    std::sort(
            msg.begin(), msg.end(),
            [](tracer::trace const& a, tracer::trace const& b) {
                auto r_hcmp = compare_hierarchy(a.hierarchy, b.hierarchy);
                if (r_hcmp == hierarchy_compare_result::equal)
                {
                    return a.unique_order < b.unique_order;
                }
                if (r_hcmp == hierarchy_compare_result::irrelevant)
                {
                    return a.unique_order < b.unique_order;
                }
                if (r_hcmp == hierarchy_compare_result::a_contains_b)
                {
                    return false;
                }
                if (r_hcmp == hierarchy_compare_result::b_contains_a)
                {
                    return true;
                }

                throw;
            });
}

void tracer::trace::dump_data(std::string& s) const
{
    switch (data.index())
    {
        case 0:
            s = "[null]";
            break;

        case 1:  //<clock_type::duration,
        {
            auto count = std::chrono::duration<double>{std::get<clock_type ::duration>(data)}.count();
            s          = fmt::format("{:.4f} ms", count * 1000.);
        }
        break;

        case 2:  // int64_t,
            s = std::to_string(std::get<int64_t>(data));
            break;

        case 3:  // double,
            s = std::to_string(std::get<double>(data));
            break;

        case 4:  // std::string,
            s.clear();
            s.append("\"");
            s.append(std::get<std::string>(data));
            s.append("\"");
            break;

        case 5:  // bool,
            s = std::get<bool>(data) ? "true" : "false";
            break;

        case 6:                       // interactive_image_t;
            s = "interactive_image";  // TODO
            break;

        default:
            s = "none";
    }
}
