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
#include <future>
#include <iostream>

#include <perfkit/common/assert.hxx>
#include <perfkit/common/thread/notify_queue.hxx>
#include <perfkit/detail/base.hpp>
#include <perfkit/detail/commands.hpp>
#include <perfkit/extension/net.hpp>
#include <perfkit/terminal.h>
#include <spdlog/spdlog.h>

#include "contexts/config_watcher.hpp"
#include "contexts/graphics_watcher.hpp"
#include "contexts/trace_watcher.hpp"
#include "dispatcher.hpp"
#include "perfkit/extension/net-internals/messages.hpp"
#include "utils.hpp"

namespace perfkit::terminal::net {
using namespace std::literals;

class terminal : public if_terminal
{
    using init_info = terminal_init_info;

   public:
    explicit terminal(init_info const& init);

    ~terminal()
    {
        detail::input_rollback();

        _active.store(false), _touch_worker();
        _worker.joinable() && (_worker.join(), 0);
        _worker_user_command.joinable() && (_worker_user_command.join(), 0);
    }

    commands::registry* commands() override
    {
        return &_commands;
    }

    std::optional<std::string> fetch_command(milliseconds timeout) override
    {
        return _command_queue.try_pop(timeout);
    }

    void push_command(std::string_view command) override
    {
        _command_queue.emplace(command);
    }

    void write(std::string_view str, termcolor fg, termcolor bg) override
    {
        detail::write(str.data(), str.size());
    }

    std::shared_ptr<spdlog::sinks::sink> sink() override
    {
        return spdlog::default_logger_raw()->sinks().front();
    }

   private:
    void _user_command_fetch_fn();
    void _char_handler(char c);
    void _on_push_command(incoming::push_command&& s);
    void _on_configure(incoming::configure_entity&& s);
    void _on_suggest_request(incoming::suggest_command&& s);
    void _on_any_connection(int n_conn);
    void _on_trace_signal(incoming::signal_fetch_traces&& s);
    void _on_trace_tweak(incoming::control_trace&& s);
    void _on_no_connection();

    void _touch_worker()
    {
        std::lock_guard lc{_mtx_worker};
        _dirty = true;
        _cvar_worker.notify_one();
    }

    void _exec();
    void _transition(std::function<void()> fn);

    // when there's no connection alive.
    void _worker_idle();

    // any new connection has been established.
    void _worker_boostrap();

    // typical loop.
    void _worker_exec();

    void _worker_cleanup() {}

   private:
    static spdlog::logger* CPPH_LOGGER() { return &*detail::nglog(); }

   private:
    outgoing::session_reset _init_msg;

    dispatcher _io;
    commands::registry _commands;

    std::atomic_bool _active = true;

    std::thread _worker_user_command;
    notify_queue<std::string> _command_queue;

    outgoing::shell_output _shell_buffered;
    std::string _shell_accumulated;
    std::atomic_flag _shell_active;

    alignas(64) std::atomic_bool _any_connection = false;
    std::thread _worker;
    std::function<void()> _worker_state;

    std::mutex _mtx_worker;
    std::condition_variable _cvar_worker;
    volatile bool _dirty;
    std::atomic_bool _new_connection_exist;

    std::pair<size_t, size_t> _bytes_io_prev;
    perfkit::stopwatch _bytes_io_counter;

    struct _context_t
    {
        context::config_watcher configs;
        context::trace_watcher traces;
        context::graphics_watcher graphics;
    } _context;
};

}  // namespace perfkit::terminal::net