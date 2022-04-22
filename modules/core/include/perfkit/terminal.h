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

#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include <spdlog/fwd.h>

#include "cpph/array_view.hxx"
#include "perfkit/detail/fwd.hpp"

namespace perfkit {
namespace commands {
class registry;
}

class if_terminal;
using terminal_ptr = std::shared_ptr<if_terminal>;
using std::chrono::milliseconds;

/** An exception that is thrown when user requests program termination */
struct termination : std::exception {};

/**
 * Provides common user interface functionality for control purpose
 */
class if_terminal : public std::enable_shared_from_this<if_terminal>
{
   private:
    struct impl;
    std::unique_ptr<impl> _self;

   public:
    if_terminal();
    virtual ~if_terminal();

   public:
    /**
     * Reference to registered command registry.
     *
     * @return reference to registered command registry.
     */
    commands::registry* commands();

    /**
     * Invoke command
     */
    virtual void invoke_command(std::string s);

    /**
     * Consume single command from user command input queue.
     *
     * @param timeout seconds to wait until receive command.
     * @return valid optional string, if dequeueing was successful.
     */
    virtual std::optional<std::string> fetch_command(milliseconds timeout = {}) = 0;

    /**
     * Enqueue command to internal queue.
     *
     * This should appear in fetch_command();
     */
    virtual void push_command(std::string_view command) = 0;

    /**
     * Output string to terminal
     */
    virtual void write(std::string_view str) = 0;

    /**
     *
     * @return number of processed commands
     */
    size_t invoke_queued_commands(milliseconds timeout = {});

    /**
     * Launch asynchronous stdin fetcher thread.
     *
     * STDIN input will redirected to 'push_command()'
     */
    void launch_stdin_fetch_thread();

   private:
    using cmd_args_view = array_view<std::string_view>;
    using cmd_invoke_function = std::function<bool(cmd_args_view full_tokens)>;

    void _add_subcommand(std::string name, cmd_invoke_function handler);

   public:
    /**
     * Helper utility
     */
    template <typename Fn_,
              typename RgTy_ = commands::registry>
    auto add_command(std::string name, Fn_&& handler = nullptr)
    {
        if constexpr (std::is_invocable_r_v<bool, Fn_, cmd_args_view>) {
            _add_subcommand(std::move(name), std::forward<Fn_>(handler));
        } else if constexpr (std::is_invocable_v<Fn_, cmd_args_view>) {
            _add_subcommand(
                    std::move(name),
                    [fn = std::forward<Fn_>(handler)](auto&& args) {
                        fn(args);
                        return true;
                    });
        } else if constexpr (std::is_invocable_v<Fn_>) {
            _add_subcommand(
                    std::move(name),
                    [fn = std::forward<Fn_>(handler)](auto&& args) {
                        fn();
                        return true;
                    });
        }
    }
};

namespace terminal {
/**
 * Register basic commands
 *
 * @param ref
 */
void initialize_with_basic_commands(if_terminal* ref);

/**
 * Register configuration load, store commands
 *
 * @param to
 * @param cmd_write usage: cmd_write [path]. if path is not specified, previous path will be used.
 * @param cmd_read usage: cmd_read [path]. if path is not specified, previous path will be used.
 */
void register_conffile_io_commands(
        if_terminal* ref,
        std::string_view cmd_load = "load-config",  // e.g. "ld"
        std::string_view cmd_store = "save-config",
        std::string_view initial_path = {});  // e.g. "w"

/**
 * Register option manipulation command
 *
 * @param to
 * @param cmd
 *
 * @details
 *
 *      <cmd> <config>
 *      <cmd> <config> set [values...]
 *      <cmd> <config:bool> toggle
 *      <cmd> <config:bool> detail
 *      <cmd> *<category-name>
 *
 */
void register_config_manip_command(
        if_terminal* ref,
        std::string_view cmd = "config");

/**
 * Register trace manipulation command
 *
 * @param ref
 * @param cmd
 *
 * @details
 *
 *      <cmd> <trace root> get
 *      <cmd> <trace> subscribe
 */
void register_trace_manip_command(
        if_terminal* ref,
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
        if_terminal* ref,
        std::string_view cmd = "logging");

}  // namespace terminal
}  // namespace perfkit