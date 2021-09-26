//
// Created by Seungwoo on 2021-08-27.
//
#include "perfkit/detail/commands.hpp"

#include <algorithm>
#include <cassert>
#include <regex>

#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>
#include <spdlog/spdlog.h>

#include "perfkit/perfkit.h"
#include "spdlog/fmt/bundled/ranges.h"

namespace {
const static std::regex rg_argv_token{R"RG((?:"((?:\\.|[^"\\])*)"|((?:[^\s\\]|\\.)+)))RG"};
const static std::regex rg_cmd_token{R"(^\S(.*\S|$))"};
}  // namespace

perfkit::commands::registry::node* perfkit::commands::registry::node::add_subcommand(
        std::string cmd,
        handler_fn handler,
        autocomplete_suggest_fn suggest,
        bool name_constant) {
  if (_check_name_exist(cmd)) {
    glog()->error("command name [{}] already exists as command or token.", cmd);
    throw command_already_exist_exception{};
  }

  std::cmatch match;
  if (!std::regex_match(cmd.c_str(), cmd.c_str() + cmd.size(), match, rg_cmd_token)) {
    glog()->error("invalid command name [{}])", cmd);
    throw command_name_invalid_exception{};
  }

  auto& subcmd          = _subcommands[std::move(cmd)];
  subcmd._invoke        = std::move(handler);
  subcmd._suggest       = std::move(suggest);
  subcmd._constant_name = name_constant;
  subcmd._parent        = this;

  return &subcmd;
}

bool perfkit::commands::registry::node::_check_name_exist(std::string_view cmd) const noexcept {
  if (auto it = _subcommands.find(cmd); it != _subcommands.end()) {
    return true;
  }
  if (auto it = _aliases.find(cmd); it != _aliases.end()) {
    return true;
  }

  return false;
}

perfkit::commands::registry::node const* perfkit::commands::registry::node::find_subcommand(std::string_view cmd_or_alias) const {
  using namespace ranges;

  if (auto it = _subcommands.find(cmd_or_alias); it != _subcommands.end()) {
    return &it->second;
  }

  if (auto it = _aliases.find(cmd_or_alias); it != _aliases.end()) {
    return &_subcommands.find(cmd_or_alias)->second;
  }

  // find initial and unique match ...
  node const* subcmd = {};
  {
    auto it = _subcommands.lower_bound(cmd_or_alias);
    for (; it != _subcommands.end() && it->first.find(cmd_or_alias) == 0; ++it) {
      if (subcmd) { return nullptr; }
      subcmd = find_subcommand(it->first);
    }
  }
  {
    auto it = _aliases.lower_bound(cmd_or_alias);
    for (; it != _aliases.end() && it->first.find(cmd_or_alias) == 0; ++it) {
      if (subcmd) { return nullptr; }
      subcmd = find_subcommand(it->first);
    }
  }

  return subcmd;
}

perfkit::commands::registry::node* perfkit::commands::registry::node::find_subcommand(std::string_view cmd_or_alias) {
  auto n = static_cast<registry::node const*>(this)->find_subcommand(cmd_or_alias);
  return const_cast<registry::node*>(n);
}

bool perfkit::commands::registry::node::erase_subcommand(std::string_view cmd_or_alias) {
  if (auto it = _subcommands.find(cmd_or_alias); it != _subcommands.end()) {
    // erase all bound aliases to given command
    for (auto it_a = _aliases.begin(); it_a != _aliases.end();) {
      auto& [alias, cmd] = *it_a;
      if (cmd == cmd_or_alias) {
        _aliases.erase(it_a++);
      } else {
        ++it_a;
      }
    }

    _subcommands.erase(it);
    return true;
  }
  if (auto it = _aliases.find(cmd_or_alias); it != _aliases.end()) {
    _aliases.erase(it);
    return true;
  }

  return false;
}

bool perfkit::commands::registry::node::alias(
        std::string_view cmd, std::string alias) {
  if (_check_name_exist(alias)) {
    glog()->error("alias name [{}] already exists as command or token.");
    return false;
  }

  if (auto it = _subcommands.find(cmd); it == _subcommands.end()) {
    glog()->error("target command [{}] does not exist.");
    return false;
  }

  _aliases.try_emplace(alias, cmd);
  return true;
}

namespace {
template <typename Rng_>
bool check_unique_prefix(std::string_view cmp, Rng_&& candidates) {
  for (const auto& candidate : candidates) {
    if (cmp.size() > candidate.size()) { continue; }   // not consider.
    if (candidate.size() == cmp.size()) { continue; }  // maybe same, or not considerable.
    if (candidate.find(cmp) == 0) { return false; }    // cmp is part of candidates
  }

  return true;
}
}  // namespace

std::string perfkit::commands::registry::node::suggest(
        perfkit::array_view<std::string_view> full_tokens,
        std::vector<std::string>& out_candidates,
        bool space_after_last_token,
        int* target_token_index,
        bool* out_has_unique_match) const {
  using namespace ranges;
  string_set user_candidates;

  if (full_tokens.empty()) {
    out_candidates |= actions::push_back(_subcommands | views::keys);
    out_candidates |= actions::push_back(_aliases | views::keys);

    if (_suggest) {
      _suggest(full_tokens, user_candidates);
      out_candidates.insert(out_candidates.end(),
                            std::move(user_candidates).begin(),
                            std::move(user_candidates).end());
    }

    return {};
  }

  auto exec          = full_tokens[0];
  bool is_last_token = full_tokens.size() == 1;

  if (node const* subcmd = {}; !is_last_token) {
    subcmd = find_subcommand(exec);

    if (subcmd) {
      // evaluate as next command, only when unique subcommand is found.

      out_has_unique_match && (*out_has_unique_match = check_unique_prefix(exec, out_candidates));
      target_token_index && ++*target_token_index;
      return subcmd->suggest(full_tokens.subspan(1),
                             out_candidates,
                             space_after_last_token,
                             target_token_index,
                             out_has_unique_match);
    }
  }

  // find default suggestion list by rule.
  //
  // note: maps are already sorted, thus one don't need to iterate all elements, but
  // simply find lower bound then iterate until meet one doesn't start with compared.
  auto filter_starts_with = [&](auto&& map, auto& compare) {
    for (auto it = map.lower_bound(compare);
         it != map.end() && it->first.find(compare) == 0;
         ++it) {
      out_candidates.push_back(it->first);
    }
  };

  filter_starts_with(_subcommands, exec);
  filter_starts_with(_aliases, exec);

  if (_suggest) {
    _suggest(full_tokens, user_candidates);
    for (auto it = user_candidates.lower_bound(exec);
         it != user_candidates.end() && it->find(exec) == 0;
         ++it) {
      out_candidates.push_back(std::move(*it));
    }
  }

  if (out_candidates.empty() == false) {
    auto sharing = out_candidates.front();

    for (auto candidate : out_candidates | views::tail) {
      // look for initial occlusion of two string
      size_t j = 0;
      for (; j < std::min(sharing.size(), candidate.size()) && sharing[j] == candidate[j]; ++j) {}

      sharing = sharing.substr(0, j);
      if (sharing.empty()) { break; }
    }

    bool is_unique_match = check_unique_prefix(sharing, out_candidates);
    out_has_unique_match && (*out_has_unique_match = is_unique_match);

    if (node const* subcmd = {}; (space_after_last_token || is_unique_match)
                                 && (subcmd = find_subcommand(sharing))
                                 && _check_name_exist(exec)) {
      out_candidates.clear();
      target_token_index && ++*target_token_index;
      return subcmd->suggest(full_tokens.subspan(1),
                             out_candidates,
                             space_after_last_token,
                             target_token_index,
                             out_has_unique_match);
    }

    return std::string{sharing};
  }

  return {};
}

bool perfkit::commands::registry::node::invoke(
        perfkit::array_view<std::string_view> full_tokens) {
  if (full_tokens.size() > 0) {
    auto subcmd = find_subcommand(full_tokens[0]);
    if (subcmd) {
      return subcmd->invoke(full_tokens.subspan(1));
    }
  }

  if (!_is_interface()) { return _invoke(full_tokens); }

  glog()->debug("command not found. all arguments: {}", full_tokens);
  return false;
}

void perfkit::commands::registry::node::reset_handler(perfkit::commands::handler_fn&& fn) {
  _invoke = std::move(fn);
}

bool perfkit::commands::registry::node::rename_subcommand(std::string_view from, std::string_view to) {
  if (find_subcommand(to) != nullptr) {
    glog()->error("subcommand rename failed: name {} already exist.", to);
    return false;
  }

  if (auto it1 = _subcommands.find(from); it1 != _subcommands.end()) {
    if (it1->second._constant_name) {
      glog()->error("subcommand rename failed; node name '{}' is protected.");
      return false;
    }
    _subcommands.try_emplace(std::string{to}, std::move(it1->second));
    _subcommands.erase(it1);
  } else if (auto it2 = _aliases.find(from); it2 != _aliases.end()) {
    _aliases.try_emplace(std::string{to}, it2->second);
    _aliases.erase(it2);
  } else {
    glog()->error("subcommand rename failed: command {} does not exist.");
    return false;
  }

  return true;
}

void perfkit::commands::registry::node::clear() {
  _subcommands.clear();
  _aliases.clear();
}

void perfkit::commands::tokenize_by_argv_rule(
        std::string* io,
        std::vector<std::string_view>& tokens,
        std::vector<stroffset>* token_indexes) {
  auto const src = *io;
  io->clear(), io->reserve(src.size());

  std::cregex_iterator iter{src.data(), src.data() + src.size(), rg_argv_token};
  std::cregex_iterator iter_end{};

  for (; iter != iter_end; ++iter) {
    auto& match = *iter;
    if (!match.ready()) { continue; }

    size_t n                = 0, position, length;
    bool wrapped_with_quote = false;
    if (match[1].matched) {
      n                  = 1;
      wrapped_with_quote = true;
    } else if (match[2].matched) {
      n                  = 2;
      wrapped_with_quote = false;
    } else {
      throw;
    }

    position = match.position(n), length = match.length(n);

    if (token_indexes) {
      token_indexes->push_back({position, length, wrapped_with_quote});
    }

    // correct escapes
    std::string str = src.substr(position, length);
    const static std::regex rg_escape{R"(\\([ \\]))"};

    auto end = std::regex_replace(str.begin(), str.begin(), str.end(), rg_escape, "$1");
    str.resize(end - str.begin());

    tokens.emplace_back(io->c_str() + io->size(), str.size());
    io->append(str);
  }
}

bool perfkit::commands::registry::invoke_command(std::string command) {
  for (const auto& [_, hook] : _invoke_hooks) {
    if (!hook(command)) { return false; }
  }

  std::vector<std::string_view> tokens;
  tokenize_by_argv_rule(&command, tokens, nullptr);

  return root()->invoke(tokens);
}

intptr_t perfkit::commands::registry::add_invoke_hook(std::function<bool(std::string&)> hook) {
  assert(hook);

  static intptr_t _idgen = 0;
  auto id                = ++_idgen;
  _invoke_hooks.emplace_back(id, std::move(hook));
  return id;
}

bool perfkit::commands::registry::remove_invoke_hook(intptr_t id) {
  using namespace ranges;
  auto names = _invoke_hooks | views::keys;

  auto it = find(names, id);
  if (it == names.end()) { return false; }

  auto index = it - names.begin();
  _invoke_hooks.erase(_invoke_hooks.begin() + index);

  return true;
}
