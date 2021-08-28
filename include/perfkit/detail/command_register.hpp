//
// Created by Seungwoo on 2021-08-27.
//
#pragma once
#include <functional>
#include <map>
#include <memory>

#include "perfkit/detail/array_view.hxx"

namespace perfkit::cmdutils {
/**
 * Tokenize given string with os argc-argv rule.
 *
 * @param src
 * @param tokens
 */
void tokenize_by_argv_rule(std::string_view src, std::vector<std::string_view>& tokens);

};  // namespace perfkit::cmdutils

namespace perfkit::ui {

/**
 *
 */
using handler_fn = std::function<bool(
    array_view<std::string_view> full_tokens,
    size_t                       this_command,
    std::string_view             arguments_string)>;

/**
 * When this handler is called, out_candidates parameter will hold initial
 * autocomplete list consist of available commands and aliases.
 */
using autocomplete_suggest_fn = std::function<void(
    array_view<std::string_view>   full_tokens,
    size_t                         current_command,
    std::vector<std::string_view>& out_candidates)>;

class command_register {
 public:
 public:
  class node {
   public:
    /**
     * Create new subcommand.
     *
     * @param cmd A command token, which must not contain any space character.
     * @param handler Command invocation handler
     * @param suggest Autocomplete suggest handler.
     * @return nullptr if given command is invalid.
     */
    perfkit::ui::command_register::node* subcommand(
        std::string_view        cmd,
        handler_fn              handler,
        autocomplete_suggest_fn suggest);

    /**
     * Find subcommand of current node.
     *
     * @param cmd_or_alias
     * @return
     */
    node* find_subcommand(std::string_view cmd_or_alias);

    /**
     * Erase subcommand.
     *
     * @param cmd_or_alias
     * @return
     */
    bool erase_subcommand(std::string_view cmd_or_alias);

    /**
     * Alias command.
     *
     * @param cmd
     * @param alias
     * @return
     */
    bool alias(std::string_view cmd, std::string alias);

    /**
     * Suggest next command based on current tokens.
     *
     * @param full_tokens
     * @param current_command
     * @param out_candidates
     */
    void suggest(
        array_view<std::string_view>   full_tokens,
        size_t                         current_command,
        std::vector<std::string_view>& out_candidates);

    /**
     * Invoke command with given arguments.
     *
     * @param full_tokens
     * @param this_command
     * @param arguments_string
     */
    bool invoke(
        array_view<std::string_view> full_tokens,
        size_t                       this_command,
        std::string_view             arguments_string);

   private:
    bool _check_name_exist(std::string_view) const noexcept;

   private:
    std::map<std::string const, node, std::less<>>              _subcommands;
    std::map<std::string const, std::string const, std::less<>> _aliases;

    handler_fn              _invoke;
    autocomplete_suggest_fn _suggest;
  };

 public:
 public:
  node* _get_root() { return _root.get(); }

 private:
  /**
   * To prevent pointer invalidation during move, allocated on heap.
   */
  std::unique_ptr<node> _root = std::make_unique<node>();
};

}  // namespace perfkit::ui