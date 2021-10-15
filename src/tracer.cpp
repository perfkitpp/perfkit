//
// Created by Seungwoo on 2021-08-25.
//
#include "perfkit/detail/tracer.hpp"

#include <future>
#include <mutex>
#include <variant>

#include <nlohmann/detail/conversions/from_json.hpp>

#include "perfkit/common/hasher.hxx"
#include "perfkit/detail/trace_future.hpp"
#include "spdlog/fmt/fmt.h"

using namespace std::literals;

using namespace perfkit;

struct tracer::_impl {
  std::optional<std::promise<async_trace_result>> msg_promise;
  tracer_future_result msg_future;
};

tracer::_entity_ty* tracer::_fork_branch(
        _entity_ty const* parent, std::string_view name, bool initial_subscribe_state) {
  auto hash = _hash_active(parent, name);

  auto [it, is_new] = _table.try_emplace(hash);
  auto& data        = it->second;

  if (is_new) {
    data.key_buffer          = std::string(name);
    data.body.hash           = hash;
    data.body.key            = data.key_buffer;
    data.body._is_subscribed = &data.is_subscribed;
    data.is_subscribed.store(initial_subscribe_state, std::memory_order_relaxed);
    parent && (data.hierarchy = parent->hierarchy, true);  // only includes parent hierarchy.
    data.hierarchy.push_back(data.key_buffer);
    data.body.hierarchy = data.hierarchy;
  }

  data.body.fence = _fence_active;
  data.body.order = _order_active++;

  return &data;
}

uint64_t tracer::_hash_active(_entity_ty const* parent, std::string_view top) {
  // --> 계층은 전역으로 관리되면 안 됨 ... 각각의 프록시가 관리해야함!!
  // Hierarchy 각각의 데이터 엔티티 기반으로 관리되게 ... _hierarchy_hash 관련 기능 싹 갈아엎기

  auto hash = hasher::FNV_OFFSET_BASE;
  if (parent) {
    hash = parent->body.hash;
  }

  for (auto c : top) { hash = hasher::fnv1a_byte(c, hash); }
  return hash;
}

tracer::proxy tracer::fork(const std::string& n) {
  if (auto _lck = std::unique_lock{_sort_merge_lock}) {
    // perform queued sort-merge operation
    if (self->msg_promise) {
      auto& promise = *self->msg_promise;

      // copies all messages and put them to cache buffer to prevent memory reallocation
      _local_reused_memory.resize(_table.size());
      std::transform(_table.begin(), _table.end(),
                     _local_reused_memory.begin(),
                     [](std::pair<const uint64_t, _entity_ty> const& g) { return g.second.body; });

      auto self_ptr = _self_weak.lock();

      async_trace_result rs = {};
      rs._mtx_access        = decltype(rs._mtx_access){self_ptr, &_sort_merge_lock};
      rs._data              = decltype(rs._data){self_ptr, &_local_reused_memory};
      promise.set_value(rs);

      self->msg_future  = {};
      self->msg_promise = {};
    }
  }

  // init new iteration
  _fence_active++;
  _order_active = 0;

  proxy prx;
  prx._owner             = this;
  prx._ref               = _fork_branch(nullptr, n, false);
  prx._epoch_if_required = clock_type::now();

  return prx;
}

namespace {
struct message_block_sorter {
  int n;
  friend bool operator<(std::weak_ptr<tracer> ptr, message_block_sorter s) {
    return s.n > ptr.lock()->order();
  }
};
}  // namespace

static auto lock_tracer_repo = [] {
  static std::mutex _lck;
  return std::unique_lock{_lck};
};

tracer::tracer(int order, std::string_view name) noexcept
        : self(new _impl), _occurence_order(order), _name(name) {
}

std::vector<std::weak_ptr<tracer>>& tracer::_all() noexcept {
  static std::vector<std::weak_ptr<tracer>> inst;
  return inst;
}

std::vector<std::shared_ptr<tracer>> tracer::all() noexcept {
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

void tracer::async_fetch_request(tracer::future_result* out) {
  auto _lck = std::unique_lock{_sort_merge_lock};

  // if there's already queued merge operation, share future once more.
  if (self->msg_promise) {
    *out = self->msg_future;
    return;
  }

  // if not, queue new promise and make its future shared.
  self->msg_promise.emplace();
  self->msg_future = std::shared_future(self->msg_promise->get_future());
  *out             = self->msg_future;
}

auto perfkit::tracer::create(int order, std::string_view name) noexcept -> std::shared_ptr<tracer> {
  auto _{lock_tracer_repo()};
  std::shared_ptr<tracer> entity{new tracer{order, name}};
  entity->_self_weak = entity;

  auto it_insert = std::lower_bound(_all().begin(), _all().end(), message_block_sorter{order});
  _all().insert(it_insert, entity);
  return entity;
}

perfkit::tracer::~tracer() noexcept {
  auto _{lock_tracer_repo()};
  auto it = std::find_if(_all().begin(), _all().end(),
                         [&](auto&& wptr) { return wptr.owner_before(_self_weak); });
  if (it != _all().end()) { _all().erase(it); }
};

tracer::proxy tracer::proxy::branch(std::string_view n) noexcept {
  proxy px;
  px._owner = _owner;
  px._ref   = _owner->_fork_branch(_ref, n, false);
  return px;
}

tracer::proxy tracer::proxy::timer(std::string_view n) noexcept {
  proxy px;
  px._owner             = _owner;
  px._ref               = _owner->_fork_branch(_ref, n, false);
  px._epoch_if_required = clock_type::now();
  return px;
}

tracer::proxy::~proxy() noexcept {
  if (!_owner) { return; }
  if (_epoch_if_required != clock_type::time_point{}) {
    data() = clock_type::now() - _epoch_if_required;
  }
}

tracer::variant_type& tracer::proxy::data() noexcept {
  return _ref->body.data;
}

void tracer::async_trace_result::copy_sorted(fetched_traces& out) const noexcept {
  {
    auto [lck, ptr] = acquire();
    out             = *ptr;
  }
  sort_messages_by_rule(out);
}

namespace {
enum class hierarchy_compare_result {
  irrelevant,
  a_contains_b,
  b_contains_a,
  equal
};

auto compare_hierarchy(array_view<std::string_view> a, array_view<std::string_view> b) {
  size_t num_equals = 0;

  for (size_t i = 0, n_max = std::min(a.size(), b.size()); i < n_max; ++i) {
    auto r_cmp = a[i].compare(b[i]);
    if (r_cmp != 0) { break; }
    ++num_equals;
  }

  if (a.size() == b.size()) {
    return num_equals == a.size()
                 ? hierarchy_compare_result::equal
                 : hierarchy_compare_result::irrelevant;
  }
  if (num_equals != a.size() && num_equals != b.size()) {
    return hierarchy_compare_result::irrelevant;
  }

  if (num_equals == a.size()) {
    return hierarchy_compare_result::b_contains_a;
  } else {
    return hierarchy_compare_result::a_contains_b;
  }
}

}  // namespace

void perfkit::sort_messages_by_rule(tracer::fetched_traces& msg) noexcept {
  std::sort(
          msg.begin(), msg.end(),
          [](tracer::trace const& a, tracer::trace const& b) {
            auto r_hcmp = compare_hierarchy(a.hierarchy, b.hierarchy);
            if (r_hcmp == hierarchy_compare_result::equal) {
              return a.fence == b.fence
                           ? a.order < b.order
                           : a.fence < b.fence;
            }
            if (r_hcmp == hierarchy_compare_result::irrelevant) {
              return a.order < b.order;
            }
            if (r_hcmp == hierarchy_compare_result::a_contains_b) {
              return false;
            }
            if (r_hcmp == hierarchy_compare_result::b_contains_a) {
              return true;
            }

            throw;
          });
}

void tracer::trace::dump_data(std::string& s) const {
  switch (data.index()) {
    case 0:  //<clock_type::duration,
    {
      auto count = std::chrono::duration<double>{std::get<clock_type ::duration>(data)}.count();
      s          = fmt::format("{:.4f}", count * 1000.);
    } break;

    case 1:  // int64_t,
      s = std::to_string(std::get<int64_t>(data));
      break;

    case 2:  // double,
      s = std::to_string(std::get<double>(data));
      break;

    case 3:  // std::string,
      s.clear();
      s.append("\"");
      s.append(std::get<std::string>(data));
      s.append("\"");
      break;

    case 4:  // bool,
      s = std::get<bool>(data) ? "true" : "false";
      break;

    case 5:                     // interactive_image_t;
      s = "interactive_image";  // TODO
      break;

    case 6:  // std::any;
      s = "--custom type--";
      break;

    default:
      s = "none";
  }
}
