//
// Created by Seungwoo on 2021-08-27.
//
#include "perfkit/detail/command_registry.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>
#include <regex>

#include "perfkit/perfkit.h"
#include "spdlog/fmt/bundled/ranges.h"

using namespace ranges;

namespace {
const static std::regex rg_argv_token{R"RG((?:"((?:\\.|[^"\\])*)"|((?:[^\s\\]|\\.)+)))RG"};
const static std::regex rg_cmd_token{R"(^\S(.*\S|$))"};
}  // namespace

perfkit::util::command_registry::node* perfkit::util::command_registry::node::add_subcommand(
        std::string_view cmd,
        handler_fn handler,
        autocomplete_suggest_fn suggest) {
  if (_check_name_exist(cmd)) {
    glog()->error("command name [{}] already exists as command or token.", cmd);
    return nullptr;
  }

  std::cmatch match;
  if (!std::regex_match(cmd.data(), cmd.data() + cmd.size(), match, rg_cmd_token)) {
    glog()->error("invalid command name [{}])", cmd);
    return nullptr;
  }

  auto& subcmd    = _subcommands[std::string(cmd)];
  subcmd._invoke  = std::move(handler);
  subcmd._suggest = std::move(suggest);

  return &subcmd;
}

bool perfkit::util::command_registry::node::_check_name_exist(std::string_view cmd) const noexcept {
  if (auto it = _subcommands.find(cmd); it != _subcommands.end()) {
    return true;
  }
  if (auto it = _aliases.find(cmd); it != _aliases.end()) {
    return true;
  }

  return false;
}

perfkit::util::command_registry::node* perfkit::util::command_registry::node::find_subcommand(std::string_view cmd_or_alias) {
  if (auto it = _subcommands.find(cmd_or_alias); it != _subcommands.end()) {
    return &it->second;
  }

  if (auto it = _aliases.find(cmd_or_alias); it != _aliases.end()) {
    return &_subcommands.find(cmd_or_alias)->second;
  }

  // find initial and unique match ...
  node* subcmd = {};
  {
    auto _keys = _subcommands | views::keys;
    auto it    = lower_bound(_keys, cmd_or_alias);
    for (; it != _keys.end() && it->find(cmd_or_alias) == 0; ++it) {
      if (subcmd) { return nullptr; }
      subcmd = find_subcommand(*it);
    }
  }
  {
    auto _keys = _aliases | views::keys;
    auto it    = lower_bound(_keys, cmd_or_alias);
    for (; it != _keys.end() && it->find(cmd_or_alias) == 0; ++it) {
      if (subcmd) { return nullptr; }
      subcmd = find_subcommand(*it);
    }
  }

  return subcmd;
}
bool perfkit::util::command_registry::node::erase_subcommand(std::string_view cmd_or_alias) {
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

bool perfkit::util::command_registry::node::alias(
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
bool check_unique(std::string_view cmp, perfkit::array_view<std::string> candidates) {
  for (const auto& candidate : candidates) {
    if (cmp.size() > candidate.size()) { continue; }   // not consider.
    if (candidate.size() == cmp.size()) { continue; }  // maybe same, or not considerable.
    if (candidate.find(cmp) == 0) { return false; }    // cmp is part of candidates
  }

  return true;
}
}  // namespace

std::string perfkit::util::command_registry::node::suggest(
        array_view<std::string_view> full_tokens,
        std::vector<std::string>& out_candidates,
        bool* out_has_unique_match) {
  std::set<std::string> user_candidates;

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

  auto exec    = full_tokens[0];
  node* subcmd = {};
  if ((subcmd = find_subcommand(exec)) && _check_name_exist(exec)) {
    // Only when full name is configured ...
    out_has_unique_match && (*out_has_unique_match = check_unique(exec, out_candidates));
    return subcmd->suggest(full_tokens.subspan(1), out_candidates, out_has_unique_match);
  }

  // find default suggestion list by rule.
  //
  // note: maps are already sorted, thus one don't need to iterate all elements, but
  // simply find lower bound then iterate until meet one doesn't start with compared.
  auto filter_starts_with = [&](auto&& keys, auto& compare, bool do_move = false) {
    for (auto it = lower_bound(keys, compare);
         it != keys.end() && it->find(compare) == 0;
         ++it) {
      do_move ? (out_candidates.push_back(*it), 0) : (out_candidates.push_back(std::move(*it)), 0);
    }
  };

  filter_starts_with(_subcommands | views::keys, exec);
  filter_starts_with(_aliases | views::keys, exec);

  if (_suggest) {
    _suggest(full_tokens, user_candidates);
    filter_starts_with(user_candidates, exec, true);
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

    out_has_unique_match && (*out_has_unique_match = check_unique(sharing, out_candidates));
    return std::string{sharing};
  }

  return {};
}

bool perfkit::util::command_registry::node::invoke(
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

void perfkit::util::command_registry::node::reset_handler(perfkit::util::handler_fn&& fn) {
  _invoke = std::move(fn);
}

void perfkit::util::tokenize_by_argv_rule(
        std::string* io,
        std::vector<std::string_view>& tokens,
        std::vector<std::pair<ptrdiff_t, size_t>>* token_indexes) {
  auto const src = *io;
  io->clear(), io->reserve(src.size());

  std::cregex_iterator iter{src.data(), src.data() + src.size(), rg_argv_token};
  std::cregex_iterator iter_end{};

  for (; iter != iter_end; ++iter) {
    auto& match = *iter;
    if (!match.ready()) { continue; }

    size_t n = 0, position, length;
    if (match[1].matched) {
      n = 1;
    } else if (match[2].matched) {
      n = 2;
    } else {
      throw;
    }

    position = match.position(n), length = match.length(n);

    if (token_indexes) {
      token_indexes->emplace_back(position, length);
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
