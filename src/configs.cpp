//
// Created by Seungwoo on 2021-08-25.
//
#include "perfkit/detail/configs.hpp"

#include <cassert>
#include <regex>

#include "perfkit/perfkit.h"
#include "spdlog/spdlog.h"

perfkit::config_registry& perfkit::config_registry::create() noexcept {
  static container _all;
  auto&            rg = *_all.emplace_back(std::make_unique<perfkit::config_registry>());
  glog()->info("Creating new config registry {}", (void*)&rg);
  return rg;
}

perfkit::config_registry::config_table& perfkit::config_registry::all() noexcept {
  static config_table _inst;
  return _inst;
}

bool perfkit::config_registry::apply_update_and_check_if_dirty() {
  decltype(_pending_updates) update;
  if (std::unique_lock _l{_update_lock}) {
    if (_pending_updates.empty()) { return false; }  // no update.

    update = std::move(_pending_updates);
    _pending_updates.clear();

    for (auto& [name, json] : update) {
      auto [key, opt] = *_opts.find(name);
      auto r_desrl    = opt->_try_deserialize(json);

      if (!r_desrl) {
        glog()->error("parse failed: '{}' <- {}", opt->display_key(), json.dump());
      }
    }

    _global_dirty.store(true, std::memory_order_relaxed);
  }

  return true;
}

static auto& key_mapping() {
  static std::map<std::string_view, std::string_view> _inst;
  return _inst;
}

void perfkit::config_registry::_put(std::shared_ptr<detail::config_base> o) {
  auto it = all().find(o->full_key());
  if (it != all().end()) { throw std::invalid_argument("Argument MUST be unique!!!"); }

  if (!find_key(o->display_key()).empty()) {
    throw std::invalid_argument(fmt::format(
        "Duplicated Display Key Found: \n\t{} (from full key {})",
        o->display_key(), o->full_key()));
  }

  key_mapping().try_emplace(o->display_key(), o->full_key());

  _opts.try_emplace(o->full_key(), o);
  all().try_emplace(o->full_key(), o);

  glog()->info("({:04}) declaring new config ... [{}] -> [{}]",
               all().size(), o->display_key(), o->full_key());
}

std::string_view perfkit::config_registry::find_key(std::string_view display_key) {
  if (auto it = key_mapping().find(display_key); it != key_mapping().end()) {
    return it->second;
  } else {
    return {};
  }
}

void perfkit::config_registry::queue_update_value(std::string full_key, const nlohmann::json& value) {
  std::unique_lock _l{_update_lock};
  _pending_updates[std::move(full_key)] = value;
}

bool perfkit::config_registry::request_update_value(std::string full_key, const nlohmann::json& value) {
  if (auto it = all().find(full_key); it != all().end()) {
    it->second->_owner->queue_update_value(std::move(full_key), value);
    return true;
  }
  return false;
}

bool perfkit::config_registry::check_dirty_and_consume_global() {
  return _global_dirty.exchange(false, std::memory_order_relaxed);
}

perfkit::detail::config_base::config_base(
    config_registry* owner,
    void* raw, std::string full_key,
    std::string                                description,
    perfkit::detail::config_base::deserializer fn_deserial,
    perfkit::detail::config_base::serializer   fn_serial)
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

  if (_full_key.back() == '|') {
    throw std::invalid_argument(
        fmt::format("Invalid Key Name: {}", _full_key));
  }

  if (_display_key.empty()) {
    throw std::invalid_argument(
        fmt::format("Invalid Generated Display Key Name: {} from full key {}",
                    _display_key, _full_key));
  }

  _split_categories(_display_key, _categories);
}

bool perfkit::detail::config_base::_try_deserialize(const nlohmann::json& value) {
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

nlohmann::json perfkit::detail::config_base::serialize() {
  nlohmann::json copy;
  return serialize(copy), copy;
}

void perfkit::detail::config_base::serialize(nlohmann::json& copy) {
  if (auto nmodify = num_modified(); _fence_serialized != nmodify) {
    auto _lock = _owner->_access_lock();
    _serialize(_cached_serialized, _raw);
    _fence_serialized = nmodify;
    copy              = _cached_serialized;
  } else {
    auto _lock = _owner->_access_lock();
    copy       = _cached_serialized;
  }
}

void perfkit::detail::config_base::serialize(std::function<void(nlohmann::json const&)> const& fn) {
  auto _lock = _owner->_access_lock();

  if (auto nmodify = num_modified(); _fence_serialized != nmodify) {
    _serialize(_cached_serialized, _raw);
    _fence_serialized = nmodify;
  }

  fn(_cached_serialized);
}

void perfkit::detail::config_base::request_modify(nlohmann::json const& js) {
  config_registry::request_update_value(std::string(full_key()), js);
}

void perfkit::detail::config_base::_split_categories(std::string_view view, std::vector<std::string_view>& out) {
  out.clear();

  // always suppose category is valid.
  // automatically excludes last token = name of it.
  for (size_t i = 0; i < view.size();) {
    if (view[i] == '|') {
      out.push_back(view.substr(0, i));
      view = view.substr(i + 1);
      i    = 0;
    } else {
      ++i;
    }
  }

  out.push_back(view);  // last segment.
}