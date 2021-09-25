//
// Created by Seungwoo on 2021-08-27.
//
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <set>

#include "perfkit/detail/array_view.hxx"

namespace perfkit::commands {
class registry;
using stroffset = std::pair<ptrdiff_t, size_t>;

struct command_exception : std::exception {};
struct command_already_exist_exception : std::exception {};

/**
 * Tokenize given string with os argc-argv rule.
 *
 * @param src
 * @param tokens
 */
void tokenize_by_argv_rule(
        std::string* io,
        std::vector<std::string_view>& tokens,
        std::vector<stroffset>* token_indexes = nullptr);

/**
 * Register configuration load, store commands
 *
 * @param to
 * @param cmd_write usage: cmd_write [path]. if path is not specified, previous path will be used.
 * @param cmd_read usage: cmd_read [path]. if path is not specified, previous path will be used.
 */
void register_config_io_commands(
        registry* ref,
        std::string_view cmd_load     = "load-config",  // e.g. "ld"
        std::string_view cmd_store    = "save-config",
        std::string_view initial_path = {});  // e.g. "w"

/**
 * Register option manipulation command
 *
 * @param to
 * @param cmd
 *
 * @details
 *
 *      <cmd> get <config>
 *      <cmd> set <config> [values...]
 *      <cmd> toggle <config:bool>
 *
 */
void register_config_manip_command(
        registry* ref,
        std::string_view cmd = "config");

/**
 * Register trace manipulation command
 *
 * @param ref
 * @param cmd
 *
 * @details
 *
 *      <cmd> get <trace root>
 *      <cmd> subscribe <trace>
 */
void register_trace_manip_command(
        registry* ref,
        std::string_view cmd = "trace");

/**
 * Register logging manipulation command
 *
 * @param ref
 * @param cmd
 *
 * @details
 *
 *      <cmd> get <logger>: returns current loglevel
 *      <cmd> set [logger]: sets loglevel of given logger. set none, applies to global
 */
void register_logging_manip_command(
        registry* ref,
        std::string_view cmd = "logging");

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

class registry {
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
    perfkit::commands::registry::node* add_subcommand(
            std::string_view cmd,
            handler_fn handler              = {},
            autocomplete_suggest_fn suggest = {});

    /**
     * Find subcommand of current node.
     *
     * @param cmd_or_alias
     * @return
     */
    node const* find_subcommand(std::string_view cmd_or_alias) const;
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
            bool* out_has_unique_match = nullptr) const;

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
  bool invoke_command(std::string command);
  node* root() { return _root.get(); }
  node const* root() const { return _root.get(); }

 public:
  node* _get_root() { return _root.get(); }

 private:
  /**
   * To prevent pointer invalidation during move, allocated on heap.
   */
  std::unique_ptr<node> _root = std::make_unique<node>();
};

}  // namespace perfkit::commands