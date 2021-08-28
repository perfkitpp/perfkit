//
// Created by Seungwoo on 2021-08-27.
//
#include "perfkit/detail/command_register.hpp"

#include <algorithm>
#include <cassert>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <range/v3/range.hpp>
#include <regex>

using namespace ranges;

namespace {
}

static bool is_valid_cmd_token(std::string_view tkn) {
  return std::regex_match(tkn.begin(), tkn.end(), std::regex{R"RG(\w+)RG"});
}

perfkit::ui::command_register::node* perfkit::ui::command_register::node::subcommand(
    std::string_view        cmd,
    handler_fn              handler,
    autocomplete_suggest_fn suggest) {
  assert(handler);

  if (_check_name_exist(cmd) || !is_valid_cmd_token(cmd)) {
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
  if (_check_name_exist(alias)) { return false; }
  if (auto it = _subcommands.find(cmd); it == _subcommands.end()) { return false; }

  _aliases.try_emplace(alias, cmd);
  return true;
}

void perfkit::ui::command_register::node::suggest(
    array_view<std::string_view>   full_tokens,
    size_t                         current_command,
    std::vector<std::string_view>& out_candidates) {
  assert(current_command < full_tokens.size());
  auto string = full_tokens[current_command];

  // find default suggestion list by rule.
  //
  // note: maps are already sorted, thus one don't need to iterate all elements, but
  // simply find lower bound then iterate until meet one doesn't start with compared.
  auto filter_starts_with = [&](auto& str_key_map, auto& compare) {
    auto keys = str_key_map | views::keys;
    for (auto it = lower_bound(keys, compare);
         it != keys.end() && it->rfind(compare) == 0;
         ++it) {
      out_candidates.push_back(*it);
    }
  };

  filter_starts_with(_subcommands, string);
  filter_starts_with(_aliases, string);

  if (_suggest) {
    _suggest(full_tokens, current_command, out_candidates);
  }
}

bool perfkit::ui::command_register::node::invoke(
    perfkit::array_view<std::string_view> full_tokens,
    size_t                                this_command,
    std::string_view                      arguments_string) {
  if (this_command + 1 < full_tokens.size()) {
    auto maybe_subcmd = full_tokens[this_command + 1];
    if (_check_name_exist(maybe_subcmd)) {
      std::string_view cmd = maybe_subcmd;

      if (auto it_alias = find_if(_aliases,
                                  [&](auto const& k) { return k.first == maybe_subcmd; });
          it_alias != _aliases.end()) {
        cmd = it_alias->second;
      }
      auto subcmd = &_subcommands.find(cmd)->second;
      return subcmd->invoke(full_tokens, this_command, arguments_string);
    }
  }

  return _invoke(full_tokens, this_command, arguments_string);
}

void perfkit::cmdutils::tokenize_by_argv_rule(std::string_view src, std::vector<std::string_view>& tokens) {
  const static std::regex rg_argv_token{R"RG((?:"((?:\\.|[^"\\])*)"|([^\s]+)))RG"};
  std::cregex_iterator    iter{src.begin(), src.end(), rg_argv_token};
  std::cregex_iterator    iter_end{};

  for (; iter != iter_end; ++iter) {
    auto& match = *iter;
    if (!match.ready()) { continue; }

    if (match[1].matched) {
      tokens.push_back(src.substr(match.position(1), match.length(1)));
    } else if (match[2].matched) {
      tokens.push_back(src.substr(match.position(2), match.length(2)));
    } else {
      throw;
    }
  }
}
