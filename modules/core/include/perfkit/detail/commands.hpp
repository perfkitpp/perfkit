// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

//
// Created by Seungwoo on 2021-08-27.
//
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>

#include "cpph/utility/array_view.hxx"
#include "perfkit/detail/fwd.hpp"

namespace perfkit::commands {
class registry;
struct stroffset {
    size_t position = {};
    size_t length = {};

    bool should_wrap = {};
};

struct command_exception : std::exception {};
struct command_already_exist_exception : std::exception {};
struct command_name_invalid_exception : std::exception {};

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
 * Invocation Handler
 */
using args_view = array_view<std::string_view>;
using invoke_fn = std::function<bool(args_view full_tokens)>;
using invoke_void_fn = std::function<void(args_view full_tokens)>;

/**
 * When this handler is called, out_candidates parameter will hold initial
 * autocomplete list consist of available commands and aliases.
 */
using string_set = std::set<std::string, std::less<>>;
using autocomplete_suggest_fn = std::function<void(args_view hint, string_set& candidates)>;

class registry
{
   public:
    class node
    {
        perfkit::commands::registry::node* _add_subcommand(
                std::string cmd,
                invoke_fn handler = {},
                autocomplete_suggest_fn suggest = {},
                bool name_constant = false);

       public:
        /**
         * Create new subcommand.
         *
         * @param cmd A command token, which must not contain any space character.
         * @param handler Command invocation handler
         * @param suggest Autocomplete suggest handler.
         * @param name_constant Disable renaming of this command.
         * @return nullptr if given command is invalid.
         */
        template <typename Fn_ = nullptr_t>
        perfkit::commands::registry::node* add_subcommand(
                std::string cmd,
                Fn_&& handler = nullptr,
                autocomplete_suggest_fn suggest = {},
                bool name_constant = false)
        {
            if constexpr (std::is_invocable_r_v<bool, Fn_, args_view>) {
                return _add_subcommand(
                        std::move(cmd), std::forward<Fn_>(handler), std::move(suggest), name_constant);
            } else if constexpr (std::is_invocable_v<Fn_, args_view>) {
                return _add_subcommand(
                        std::move(cmd),
                        [fn = std::forward<Fn_>(handler)](auto&& args) {
                            fn(args);
                            return true;
                        },
                        std::move(suggest), name_constant);
            } else if constexpr (std::is_invocable_v<Fn_>) {
                return _add_subcommand(
                        std::move(cmd),
                        [fn = std::forward<Fn_>(handler)](auto&& args) {
                            fn();
                            return true;
                        },
                        std::move(suggest), name_constant);
            } else if (std::is_same_v<Fn_, nullptr_t>) {
                return _add_subcommand(
                        std::move(cmd),
                        {},
                        std::move(suggest), name_constant);
            }
        }

        /**
         * Acquire lock for transaction
         */
        auto acquire() const { return std::unique_lock{*_subcmd_lock}; }

        /**
         * Find subcommand of current node.
         *
         * @param cmd_or_alias
         * @return
         */
        auto find_subcommand(std::string_view ss) const
        {
            return std::make_pair(
                    _find_subcommand(ss),
                    std::unique_lock{*_subcmd_lock});
        }

        auto find_subcommand(std::string_view ss)
        {
            return std::make_pair(
                    _find_subcommand(ss),
                    std::unique_lock{*_subcmd_lock});
        }

        /**
         * Check if command exists
         *
         * @param s
         * @return
         */
        bool is_valid_command(std::string_view s) const { return !!_find_subcommand(s); }

        /**
         * Erase subcommand.
         *
         * @param cmd_or_alias
         * @return
         */
        bool erase_subcommand(std::string_view cmd_or_alias);

        /**
         * Erase all subcommands
         */
        void clear();

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
        void reset_invoke_handler(invoke_fn fn);

        /**
         * Reset suggest fn
         */
        void reset_suggest_handler(autocomplete_suggest_fn fn);

        /**
         * Set operation hook which is invoked before every suggest/invoke
         */
        void reset_opreation_hook(std::function<void(node*, args_view)> hook);

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
                perfkit::array_view<std::string_view> full_tokens,
                std::vector<std::string>& out_candidates,
                bool space_after_last_token,
                int* target_token_index = nullptr,
                bool* out_has_unique_match = nullptr);

        /**
         * Invoke command with given arguments.
         *
         * @param full_tokens
         */
        bool invoke(array_view<std::string_view> full_tokens);

        /**
         * @return Parent node handle.
         */
        node* parent() const { return _parent; }

       private:
        bool _check_name_exist(std::string_view) const noexcept;
        bool _is_interface() const noexcept { return !_invoke; }

        node const* _find_subcommand(std::string_view cmd_or_alias) const;
        node* _find_subcommand(std::string_view cmd_or_alias);

       private:
        friend class registry;

        std::map<std::string const, node, std::less<>> _subcommands;
        std::map<std::string const, std::string const, std::less<>> _aliases;

        node* _parent;
        std::recursive_mutex* _subcmd_lock;

        invoke_fn _invoke;
        autocomplete_suggest_fn _suggest;
        std::function<void(node*, args_view)> _hook_pre_op;
        bool _constant_name = {};
    };

   public:
    using hook_fn = std::function<bool(std::string& command)>;

   public:
    bool invoke_command(std::string command);
    node* root() { return _root.get(); }
    node const* root() const { return _root.get(); }

    registry() { _root->_subcmd_lock = &_subcmd_lock; }

    intptr_t add_invoke_hook(hook_fn hook);
    bool remove_invoke_hook(intptr_t);

    std::string suggest(std::string line, std::vector<std::string>* candidates);

   public:
    node* _get_root() { return _root.get(); }

   private:
    // to prevent pointer invalidation during move, allocated on heap.
    std::unique_ptr<node> _root = std::make_unique<node>();

    // invocation hooks.
    std::vector<std::pair<intptr_t, hook_fn>> _invoke_hooks;

    // make all operation atomic
    std::recursive_mutex _subcmd_lock;
};

}  // namespace perfkit::commands