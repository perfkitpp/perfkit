//
// Created by Seungwoo on 2021-08-25.
//
#include <cassert>
#include <perfkit/detail/options.hpp>
#include <regex>

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
      _opts.find(pair.first)->second->_try_deserialize(pair.second);
    }
  }

  return true;
}

static auto& key_mapping() {
  static std::map<std::string_view, std::string_view> _inst;
  return _inst;
}

void perfkit::option_dispatcher::_put(std::shared_ptr<detail::option_base> o) {
  auto it = _all().find(o->full_key());
  if (it != _all().end()) { throw std::invalid_argument("Argument MUST be unique!!!"); }

  if (!find_key(o->display_key()).empty()) {
    throw std::invalid_argument("Duplicated Display Key Found");
  }

  key_mapping().try_emplace(o->display_key(), o->full_key());

  _opts.try_emplace(o->full_key(), o);
  _all().try_emplace(o->full_key(), o);
}

std::string_view perfkit::option_dispatcher::find_key(std::string_view display_key) {
    if (auto it = key_mapping().find(display_key); it != key_mapping().end()) {
      return it->second;
    } else {
      return {};
    }
}

perfkit::detail::option_base::option_base(void* raw, std::string full_key, std::string description, perfkit::detail::option_base::deserializer fn_deserial, perfkit::detail::option_base::serializer fn_serial)
    : _full_key(std::move(full_key))
    , _description(std::move(description))
    , _raw(raw)
    , _deserialize(std::move(fn_deserial))
    , _serialize(std::move(fn_serial)) {
  static std::regex rg_trim_whitespace{R"((?:^|\|?)\s*(\S?[^|]*\S)\s*(?:\||$))"};
  static std::regex rg_remove_order_marker{R"(\+[^|]+\|)"};

  _display_key.reserve(_full_key.size());
  for (std::sregex_iterator end{}, it{_full_key.begin(), _full_key.end(), rg_trim_whitespace};
       it != end;
       ++it) {
    if (!it->ready()) { throw "Invalid Token"; }
    _display_key.append(it->str(1));
    _display_key.append("|");
  }
  _display_key.pop_back();  // remove last | character

  auto it = std::regex_replace(_display_key.begin(),
                               _display_key.begin(), _display_key.end(),
                               rg_remove_order_marker, "");
  _display_key.resize(it - _display_key.begin());

  if (_full_key.back() == '|') { throw ""; }
  if (_display_key.empty()) { throw "Invalid Key"; }
}
