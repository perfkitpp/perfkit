//
// Created by Seungwoo on 2021-08-27.
//
#include "perfkit/detail/command_register.hpp"

#include <algorithm>
#include <cassert>
#include <range/v3/algorithm.hpp>
#include <regex>

using namespace ranges;

namespace {
const std::regex rg_argv_token{R"RG((?:"(\\.|[^"\\])*"|([^\s]+)))RG"};
}

static bool is_valid_cmd_token(std::string_view tkn) {
  return true;
}

perfkit::ui::command_register::node* perfkit::ui::command_register::node::subcommand(
    std::string_view cmd,
    command_register::handler_fn handler,
    command_register::autocomplete_suggest_fn suggest) {
  if (_chk_dup(cmd) || !is_valid_cmd_token(cmd)) {
    return nullptr;
  }

  auto& subcmd = _subcommands[std::string(cmd)];
  subcmd._invoke = std::move(handler);
  subcmd._suggest = std::move(suggest);

  return &subcmd;
}

bool perfkit::ui::command_register::node::_chk_dup(std::string_view cmd) const noexcept {
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
  return nullptr;
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
  if (_chk_dup(alias)) { return false; }
  if (auto it = _subcommands.find(cmd); it == _subcommands.end()) { return false; }

  _aliases.try_emplace(alias, cmd);
  return true;
}

void perfkit::ui::command_register::node::suggest(
    array_view<std::string_view> full_tokens,
    size_t current_command,
    std::vector<std::string_view>& out_candidates) {
  assert(current_command < full_tokens.size());
  out_candidates.clear();
  auto string = full_tokens[current_command];

  // find default suggestion list by rule.
  //
  // note: maps are already sorted, thus one don't need to iterate all elements, but
  // simply find lower bound then iterate until meet one doesn't start with compared.
  auto filter_starts_with = [&](auto& str_key_map, auto& compare) {
    for (auto it = lower_bound(str_key_map, compare);
         it != str_key_map.end() && str_key_map(it->first, compare) == 0;
         ++it) {
      out_candidates.push_back(it->first);
    }
  };


}
