//
// Created by Seungwoo on 2021-10-01.
//
#include <list>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>

#include "perfkit/common/format.hxx"
#include "perfkit/detail/configs.hpp"

void perfkit::configs::parse_args(int* argc, char*** argv, bool consume, bool ignore_undefined) {
  using namespace ranges;
  auto args = views::iota(0, *argc)
            | views::transform([&](auto i) -> std::string_view { return (*argv)[i]; })
            | to<std::vector<std::string_view>>();

  parse_args(&args, consume, ignore_undefined);
  assert(args.size() <= *argc);

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
      throw parse_error("failed to parse value ... {}:{} << {}\n error: {}"_fmt
                        % _conf->display_key()
                        % _conf->default_value().type_name()
                        % tok
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
                         && tok.size() >= 1
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
      str << "  {:.<60}\n"_fmt
                      % to_string("{{ \"{}\" :{} }}"_fmt
                                  % conf->display_key()
                                  % conf->default_value().type_name());
      str += "    <flags> ";
      for (auto& key : keys) { str << "{}{} "_fmt % (key.size() == 1 ? "-" : "--") % key; }
      str << "\n    <default> {}\n"_fmt % conf->default_value().dump();
      str << "    <description> {}\n\n"_fmt % (conf->description().empty() ? "<no description>" : conf->description());
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
