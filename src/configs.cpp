//
// Created by Seungwoo on 2021-08-25.
//
#include "perfkit/detail/configs.hpp"

#include <cassert>
#include <regex>

#include <perfkit/common/format.hxx>
#include <perfkit/perfkit.h>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

perfkit::config_registry& perfkit::config_registry::create() noexcept {
  static container _all;
  auto& rg = *_all.emplace_back(std::make_unique<perfkit::config_registry>());
  glog()->debug("Creating new config registry {}", (void*)&rg);
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

    for (auto ptr : update) {
      auto r_desrl = ptr->_try_deserialize(ptr->_cached_serialized);

      if (!r_desrl) {
        glog()->error("parse failed: '{}' <- {}", ptr->display_key(), ptr->_cached_serialized.dump());
        ptr->_serialize(ptr->_cached_serialized, ptr->_raw);  // roll back
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
  if (auto it = all().find(o->full_key()); it != all().end()) {
    throw std::invalid_argument("Argument MUST be unique!!!");
  }

  if (!find_key(o->display_key()).empty()) {
    throw std::invalid_argument(fmt::format(
            "Duplicated Display Key Found: \n\t{} (from full key {})",
            o->display_key(), o->full_key()));
  }

  key_mapping().try_emplace(o->display_key(), o->full_key());

  _opts.try_emplace(o->full_key(), o);
  all().try_emplace(o->full_key(), o);

  glog()->debug("({:04}) declaring new config ... [{}] -> [{}]",
                all().size(), o->display_key(), o->full_key());

  if (auto attr = &o->attribute(); attr->contains("is_flag")) {
    std::vector<std::string> bindings;
    if (auto it = attr->find("flag_binding"); it != attr->end()) {
      bindings = it->get<std::vector<std::string>>();
    } else {
      using namespace ranges;
      bindings.emplace_back(
              o->display_key()
              | views::transform([](auto c) { return c == '|' ? '.' : c; })
              | to<std::string>());
    }

    for (auto& binding : bindings) {
      static std::regex match{R"RG(^(((?!no-)[^N-][\w-]*)|N[\w-]+))RG"};
      if (!std::regex_match(binding, match)
          || binding.find_first_of(' ') != ~size_t{}
          || binding == "help" || binding == "h") {
        throw configs::invalid_flag_name(
                fmt::format("invalid flag name: {}", binding));
      }

      auto [_, is_new] = configs::_flags().try_emplace(std::move(binding), o);
      if (!is_new) {
        throw configs::duplicated_flag_binding{
                fmt::format("Binding name duplicated: {}", binding)};
      }
    }
  }
}

std::string_view perfkit::config_registry::find_key(std::string_view display_key) {
  if (auto it = key_mapping().find(display_key); it != key_mapping().end()) {
    return it->second;
  } else {
    return {};
  }
}

void perfkit::config_registry::queue_update_value(std::string_view full_key, const nlohmann::json& value) {
  auto _ = _access_lock();

  auto it  = _opts.find(full_key);
  full_key = it->first;

  // to prevent value ignorance on contiguous load-save call without apply_changes(),
  // stores cache without validation.
  it->second->_cached_serialized = value;
  _pending_updates.insert(it->second.get());
}

bool perfkit::config_registry::request_update_value(std::string_view full_key, const nlohmann::json& value) {
  if (auto it = all().find(full_key); it != all().end()) {
    it->second->_owner->queue_update_value(std::string(full_key), value);
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
        std::string description,
        perfkit::detail::config_base::deserializer fn_deserial,
        perfkit::detail::config_base::serializer fn_serial,
        nlohmann::json&& attribute)
        : _owner(owner)
        , _full_key(std::move(full_key))
        , _description(std::move(description))
        , _raw(raw)
        , _attribute(std::move(attribute))
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

void perfkit::configs::parse_args(int* argc, char*** argv, bool consume, bool ignore_undefined) {
  using namespace ranges;
  auto args = views::iota(0, *argc)
            | views::transform([&](auto i) -> std::string_view { return (*argv)[i]; })
            | to<std::vector<std::string_view>>();

  parse_args(&args, consume, ignore_undefined);
  assert(args.size() < *argc);

  *argc = static_cast<int>(args.size());
  copy(args | views::transform([](std::string_view s) {
         return const_cast<char*>(s.data());
       }),
       *argv);
}

namespace perfkit::configs::_parsing {
class if_state;
using state_ptr = std::unique_ptr<if_state>;

class if_state {
 public:
  virtual ~if_state()                                        = default;
  virtual bool invoke(std::string_view tok, if_state** next) = 0;

 protected:
  template <typename Ty_, typename... Args_>
  if_state* fork(if_state* return_state, Args_&&... args) {
    auto ptr              = state_ptr{new Ty_{std::forward<Args_>(args)...}};
    ptr->ignore_undefined = ignore_undefined;
    ptr->_return          = return_state;
    _child                = std::move(ptr);
    return _child.get();
  }

  bool do_return(bool result, if_state** next) {
    *next = _return;
    return result;
  }

 public:
  bool ignore_undefined = false;

 protected:
  if_state* _return;
  state_ptr _child;
};

config_ptr _find_conf(std::string_view name, bool ignore_undefined) {
  auto it = _flags().find(name);
  if (it != _flags().end()) { return it->second; }
  if (!ignore_undefined) { throw invalid_flag_name{"flag not exist: {}"_fmt % name}; }
  return {};
}

config_ptr _find_conf(char name, bool ignore_undefined) {
  return _find_conf(std::string_view{&name, 1}, ignore_undefined);
}

std::string_view _retrieve_key(std::string_view flag) {
  if (flag.find("no-") == 0) { flag = flag.substr(3); }
  flag = flag.substr(0, flag.find_first_of("= "));
  return flag;
}

class value_parse : public if_state {
 public:
  explicit value_parse(config_ptr conf)
          : _conf{conf} {}

  bool invoke(std::string_view tok, if_state** next) override {
    if (tok.empty()) { return *next = this, true; }  // wait for next token ...

    try {
      if (_conf->default_value().is_string()) {
        std::string parsed;
        parsed.reserve(2 + tok.size());
        parsed < R"("{}")"_fmt % tok;
        _conf->request_modify(nlohmann::json::parse(parsed));
      } else {
        _conf->request_modify(nlohmann::json::parse(tok.begin(), tok.end()));
      }
    } catch (nlohmann::json::parse_error& e) {
      throw parse_error("failed to parse value ... {} << {}\n error: {}"_fmt
                        % _conf->display_key() % tok
                        % e.what());
    }

    return do_return(true, next);
  }

 private:
  config_ptr _conf;
};

class single_dash_parse : public if_state {
 public:
  bool invoke(std::string_view tok, if_state** next) override {
    auto ch_first = tok[0];
    if (config_ptr conf; ch_first != 'N'
                         && tok.size() > 1
                         && (conf = _find_conf(ch_first, ignore_undefined))
                         && not conf->default_value().is_boolean()) {
      // it's asserted to be value string from second charater.
      return fork<value_parse>(_return, conf)->invoke(tok.substr(1), next);
    }

    bool value = ch_first != 'N';
    auto token = ch_first == 'N' ? tok.substr(1) : tok;

    if (token.empty()) { throw parse_error("invalid flag: {}"_fmt % tok); }

    for (char ch : token) {
      auto conf = _find_conf(ch, ignore_undefined);

      if (conf == nullptr) { continue; }
      if (not conf->default_value().is_boolean()) {
        throw parse_error("flag {} is not boolean"_fmt % ch);
      }

      conf->request_modify(value);
    }

    return do_return(true, next);
  }
};

class double_dash_parse : public if_state {
 public:
  bool invoke(std::string_view tok, if_state** next) override {
    auto key = _retrieve_key(tok);
    if (key.empty()) { throw parse_error("key retrieval failed: {}"_fmt % tok); }

    auto conf = _find_conf(key, ignore_undefined);
    if (conf == nullptr) { return do_return(true, next); }

    bool is_not = tok.find("no-") == 0;
    if (conf->default_value().is_boolean()) {
      conf->request_modify(not is_not);
      return do_return(true, next);
    } else if (is_not) {
      throw parse_error("invalid flag {}: {} is not boolean"_fmt % tok % key);
    } else {
      auto tok_value = tok.substr(key.size());  // strip initial '='
      if (not tok_value.empty() && tok_value[0] == '=') { tok_value = tok_value.substr(1); }
      return fork<value_parse>(_return, conf)->invoke(tok_value, next);
    }
  }
};

class initial_state : public if_state {
 public:
  bool invoke(std::string_view tok, if_state** next) override {
    if (tok.length() < 2) { return false; }
    if (tok[0] != '-') { return false; }

    if (tok == "-h" || tok == "--help") { _build_help(); }

    if (tok[1] == '-') {
      return fork<double_dash_parse>(this)->invoke(tok.substr(2), next);
    } else {
      return fork<single_dash_parse>(this)->invoke(tok.substr(1), next);
    }
  }

 private:
  void _build_help() {
    std::map<detail::config_base*, std::list<std::string_view>>
            flag_mappings;

    std::string str;

    using namespace ranges;
    str += "usage: <program> ";
    for (const auto& [key, conf] : _flags()) {
      flag_mappings[conf.get()].push_back(key);
      str << "[{}{}{}] "_fmt
                      % (key.size() == 1 ? "-" : "--")
                      % key
                      % (conf->default_value().is_boolean() ? ""
                         : key.size() == 1                  ? "<value>"
                                                            : "=<value>");
    }

    str += "args...";

    str += "\n\n"_fmt.s();
    for (const auto& [conf, keys] : flag_mappings) {
      str << "---- {:-<40}\n"_fmt % ("{} "_fmt % conf->display_key()).string();
      str += "  ";
      for (auto& key : keys) { str << "{}{} "_fmt % (key.size() == 1 ? "-" : "--") % key; }
      str << "\n    {}\n\n"_fmt % (conf->description().empty() ? "<no description>" : conf->description());
    }

    throw parse_help(std::move(str));
  }
};

}  // namespace perfkit::configs::_parsing

void perfkit::configs::parse_args(
        std::vector<std::string_view>* args, bool consume, bool ignore_undefined) {
  using namespace _parsing;

  // Parsing Rule:
  // --key=<value> --key <value> -k[value]
  // --flag --no-flag -abcd -Nabcd
  // 단일 '-' 플래그에서 아래 경우만 valid
  // - 모든 문자가 boolean flag = true 값 포워드
  // - 첫 문자가 N, 이후 문자가 모두 boolean flag = false 값 포워드
  // - 첫 문자가 non-boolean 플래그 -> 이후 문자는 값으로 파싱
  // - 첫 문자 boolean 플래그, 이후 문자 non-boolean 플래그면 에러
  // 쌍 '--' 플래그에서
  // -
  // "--" 이후 토큰은 호출자에게 넘김 ...

  state_ptr initial_state{new class initial_state};
  initial_state->ignore_undefined = ignore_undefined;
  if_state* state                 = initial_state.get();

  for (auto it = (*args).begin(); it != (*args).end(); ++it) {
    auto tok = *it;

    if (state == initial_state.get() && tok == "--") { break; }
    if (state->invoke(tok, &state) && consume) { it = args->erase(it), --it; }
  }
}

perfkit::configs::flag_binding_table& perfkit::configs::_flags() noexcept {
  static flag_binding_table _inst;
  return _inst;
}
