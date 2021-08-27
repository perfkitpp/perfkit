//
// Created by Seungwoo on 2021-08-25.
//
#include <cassert>
#include <perfkit/detail/options.hpp>

perfkit::option_dispatcher& perfkit::option_dispatcher::_create() noexcept {
  static container _all;
  return *_all.emplace_back(std::make_unique<perfkit::option_dispatcher>());
}

perfkit::option_dispatcher::option_table& perfkit::option_dispatcher::_all() noexcept {
  static option_table _inst;
  return _inst;
}

bool perfkit::option_dispatcher::apply_update_and_check_if_dirty() {
  decltype(_pending_updates) update;
  if (std::unique_lock _l{_update_lock}) {
    if (_pending_updates.empty()) { return false; }
    update = std::move(_pending_updates);
    _pending_updates.clear();

    for (auto& pair : update) {
      _opts.find(pair.first)->second->try_marshal(pair.second);
    }
  }

  return true;
}

void perfkit::option_dispatcher::_put(std::shared_ptr<detail::option_base> o) {
  auto it = _all().find(o->full_key());
  if (it != _all().end()) { throw std::invalid_argument("Argument MUST be unique!!!"); }

  _opts.try_emplace(o->full_key(), o);
  _all().try_emplace(o->full_key(), o);
}
