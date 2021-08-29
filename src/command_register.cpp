//
// Created by Seungwoo on 2021-08-27.
//
#include "perfkit/detail/command_register.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <range/v3/action/push_back.hpp>
#include <range/v3/algorithm.hpp>
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>
#include <regex>

#include "spdlog/fmt/bundled/ranges.h"

using namespace ranges;

namespace {
}

static bool is_valid_cmd_token(std::string_view tkn) {
  return std::regex_match(tkn.begin(), tkn.end(), std::regex{R"RG([\w-]+)RG"});
}

perfkit::ui::command_register::node* perfkit::ui::command_register::node::subcommand(
    std::string_view        cmd,
    handler_fn              handler,
    autocomplete_suggest_fn suggest) {
  assert(handler);

  if (_check_name_exist(cmd)) {
    spdlog::error("command name [{}] already exists as command or token.");
    return nullptr;
  }
  if (!is_valid_cmd_token(cmd)) {
    spdlog::error("invalid command name [{}]. should be expression of \\[\\w-]\\)");
    return nullptr;
  }

  auto& subcmd    = _subcommands[std::string(cmd)];
  subcmd._invoke  = std::move(handler);
  subcmd._suggest = std::move(suggest);

  return &subcmd;
}

bool perfkit::ui::command_register::node::_check_name_exist(std::string_view cmd) const noexcept {
  if (auto it = _subcommands.find(cmd); it != _subcommands.end()) {
    return true;
  }
  if (auto it = _aliases.find(cmd); it != _aliases.end()) {
    return true;
  }

  return false;
}

perfkit::ui::command_register::node* perfkit::ui::command_register::node::find_subcommand(std::string_view cmd_or_alias) {
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
bool perfkit::ui::command_register::node::erase_subcommand(std::string_view cmd_or_alias) {
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

bool perfkit::ui::command_register::node::alias(
    std::string_view cmd, std::string alias) {
  if (_check_name_exist(alias)) {
    spdlog::error("alias name [{}] already exists as command or token.");
    return false;
  }

  if (auto it = _subcommands.find(cmd); it == _subcommands.end()) {
    spdlog::error("target command [{}] does not exist.");
    return false;
  }

  _aliases.try_emplace(alias, cmd);
  return true;
}

std::string_view perfkit::ui::command_register::node::suggest(
    array_view<std::string_view>   full_tokens,
    size_t                         current_command,
    std::vector<std::string_view>& out_candidates) {
  if (current_command == ~size_t{}) {
    out_candidates |= actions::push_back(_subcommands | views::keys);
    out_candidates |= actions::push_back(_aliases | views::keys);
  } else if (current_command > 0) {
    auto subcmd = find_subcommand(full_tokens[0]);
    if (!subcmd) { return {}; }

    return subcmd->suggest(full_tokens.subspan(1), current_command - 1, out_candidates);
  } else {
    auto string = full_tokens[0];

    // find default suggestion list by rule.
    //
    // note: maps are already sorted, thus one don't need to iterate all elements, but
    // simply find lower bound then iterate until meet one doesn't start with compared.
    auto filter_starts_with = [&](auto& str_key_map, auto& compare) {
      auto keys = str_key_map | views::keys;
      for (auto it = lower_bound(keys, compare);
           it != keys.end() && it->find(compare) == 0;
           ++it) {
        out_candidates.push_back(*it);
      }
    };

    filter_starts_with(_subcommands, string);
    filter_starts_with(_aliases, string);
  }

  if (_suggest) {
    _suggest(full_tokens, current_command, out_candidates);
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

    return sharing;
  }

  return {};
}

bool perfkit::ui::command_register::node::invoke(
    perfkit::array_view<std::string_view> full_tokens) {
  if (full_tokens.size() > 0) {
    auto subcmd = find_subcommand(full_tokens[0]);
    if (subcmd) {
      return subcmd->invoke(full_tokens.subspan(1));
    }
  }

  if (!_is_root()) { return _invoke(full_tokens); }

  spdlog::error("command {} not found. all arguments: {}", full_tokens[0], full_tokens);
  return false;
}

void perfkit::cmdutils::tokenize_by_argv_rule(
    std::string_view                           src,
    std::vector<std::string>&                  tokens,
    std::vector<std::pair<ptrdiff_t, size_t>>* token_indexes) {
  const static std::regex rg_argv_token{R"RG((?:"((?:\\.|[^"\\])*)"|((?:[^\s\\]|\\.)+)))RG"};
  std::cregex_iterator    iter{src.begin(), src.end(), rg_argv_token};
  std::cregex_iterator    iter_end{};

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
    tokens.push_back(std::string(src.substr(position, length)));

    if (token_indexes) {
      token_indexes->emplace_back(position, length);
    }

    // correct escapes
    auto&                   str = tokens.back();
    const static std::regex rg_escape{R"(\\([ \\]))"};

    auto end = std::regex_replace(str.begin(), str.begin(), str.end(), rg_escape, "$1");
    str.resize(end - str.begin());
  }
}
