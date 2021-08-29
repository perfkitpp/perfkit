//
// Created by Seungwoo on 2021-08-25.
//
#include <cassert>
#include <perfkit/detail/options.hpp>
#include <regex>

perfkit::option_registry& perfkit::option_registry::_create() noexcept {
  static container _all;
  return *_all.emplace_back(std::make_unique<perfkit::option_registry>());
}

perfkit::option_registry::option_table& perfkit::option_registry::all() noexcept {
  static option_table _inst;
  return _inst;
}

bool perfkit::option_registry::apply_update_and_check_if_dirty() {
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

void perfkit::option_registry::_put(std::shared_ptr<detail::option_base> o) {
  auto it = all().find(o->full_key());
  if (it != all().end()) { throw std::invalid_argument("Argument MUST be unique!!!"); }

  if (!find_key(o->display_key()).empty()) {
    throw std::invalid_argument("Duplicated Display Key Found");
  }

  key_mapping().try_emplace(o->display_key(), o->full_key());

  _opts.try_emplace(o->full_key(), o);
  all().try_emplace(o->full_key(), o);
}

std::string_view perfkit::option_registry::find_key(std::string_view display_key) {
  if (auto it = key_mapping().find(display_key); it != key_mapping().end()) {
    return it->second;
  } else {
    return {};
  }
}
void perfkit::option_registry::queue_update_value(std::string full_key, const nlohmann::json& value) {
  std::unique_lock _l{_update_lock};
  _pending_updates[std::move(full_key)] = value;
}

perfkit::detail::option_base::option_base(
    option_registry* owner,
    void* raw, std::string full_key,
    std::string                                description,
    perfkit::detail::option_base::deserializer fn_deserial,
    perfkit::detail::option_base::serializer   fn_serial)
    : _owner(owner)
    , _full_key(std::move(full_key))
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

bool perfkit::detail::option_base::_try_deserialize(const nlohmann::json& value) {
  if (_deserialize(value, _raw)) {
    _fence_modification.fetch_add(1, std::memory_order_relaxed);
    _dirty = true;

    _latest_marshal_failed.store(false, std::memory_order_relaxed);
    return true;
  } else {
    _latest_marshal_failed.store(true, std::memory_order_relaxed);
    return false;
  }
}

nlohmann::json const& perfkit::detail::option_base::serialize() {
  if (auto nmodify = num_modified(); _fence_serialized != nmodify) {
    auto _lock = _owner->_access_lock();
    _serialize(_cached_serialized, _raw);
    _fence_serialized = nmodify;
  }
  return _cached_serialized;
}
