//
// Created by Seungwoo on 2021-08-27.
//
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "perfkit/detail/array_view.hxx"

namespace perfkit::util {
using stroffset = std::pair<ptrdiff_t, size_t>;

/**
 * Tokenize given string with os argc-argv rule.
 *
 * @param src
 * @param tokens
 */
void tokenize_by_argv_rule(std::string* io,
                           std::vector<std::string_view>& tokens,
                           std::vector<stroffset>* token_indexes = nullptr);

};  // namespace perfkit::cmdutils

namespace perfkit::util {

/**
 * Invocation Handler
 */
using args_view  = array_view<std::string_view>;
using handler_fn = std::function<bool(args_view full_tokens)>;

/**
 * When this handler is called, out_candidates parameter will hold initial
 * autocomplete list consist of available commands and aliases.
 */
using string_set              = std::set<std::string>;
using autocomplete_suggest_fn = std::function<void(args_view hint, string_set& candidates)>;

class command_registry {
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
    perfkit::util::command_registry::node* add_subcommand(
        std::string_view cmd,
        handler_fn handler              = {},
        autocomplete_suggest_fn suggest = {});

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
     * Reset handler with given argument.
     */
    void reset_handler(handler_fn&&);

    /**
     * Suggest next command based on current tokens.
     *
     * @param full_tokens
     * @param current_command
     * @param out_candidates
     *
     * @return Common parts of given suggestions, which is used to smart autocomplete
     */
    std::string suggest(
        array_view<std::string_view> full_tokens,
        std::vector<std::string>& out_candidates,
        bool* out_has_unique_match = nullptr);

    /**
     * Invoke command with given arguments.
     *
     * @param full_tokens
     */
    bool invoke(array_view<std::string_view> full_tokens);

   private:
    bool _check_name_exist(std::string_view) const noexcept;
    bool _is_interface() const noexcept { return !_invoke; }

   private:
    std::map<std::string const, node, std::less<>> _subcommands;
    std::map<std::string const, std::string const, std::less<>> _aliases;

    handler_fn _invoke;
    autocomplete_suggest_fn _suggest;
  };

 public:
  node* _get_root() { return _root.get(); }

 private:
  /**
   * To prevent pointer invalidation during move, allocated on heap.
   */
  std::unique_ptr<node> _root = std::make_unique<node>();
};

}  // namespace perfkit::ui