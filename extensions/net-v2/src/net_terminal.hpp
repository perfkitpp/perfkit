/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2022. Seungwoo Kang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * project home: https://github.com/perfkitpp
 ******************************************************************************/

//
// Created by ki608 on 2022-03-06.
//

#pragma once
#include <unordered_set>
#include <utility>

#include <asio/ip/tcp.hpp>
#include <asio/thread_pool.hpp>

#include "context/config_context.hpp"
#include "perfkit/common/circular_queue.hxx"
#include "perfkit/common/hasher.hxx"
#include "perfkit/common/refl/msgpack-rpc/context.hxx"
#include "perfkit/common/thread/locked.hxx"
#include "perfkit/common/thread/notify_queue.hxx"
#include "perfkit/common/thread/worker.hxx"
#include "perfkit/detail/commands.hpp"
#include "perfkit/extension/net/protocol.hpp"
#include "perfkit/logging.h"
#include "perfkit/terminal.h"

namespace perfkit::net {
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::vector;
using std::weak_ptr;

struct terminal_info
{
    string   name;
    string   description;

    string   bind_ip;
    uint16_t bind_port;

    // TODO: Authentication
};

class terminal_monitor : public msgpack::rpc::if_context_monitor
{
    class terminal*     _owner;
    perfkit::logger_ptr _logger;

   public:
    explicit terminal_monitor(terminal* owner, logger_ptr logger)
            : _owner(owner), _logger(std::move(logger)) {}

   public:
    void on_new_session(const msgpack::rpc::session_profile& profile) noexcept override;
    void on_dispose_session(const msgpack::rpc::session_profile& profile) noexcept override;

   private:
    auto CPPH_LOGGER() const { return &*_logger; }
};

class terminal : public if_terminal
{
    friend class terminal_monitor;

    //
    terminal_info _info;

    // Thread pool
    asio::io_context  _event_proc;
    asio::thread_pool _ioc{4};
    thread::worker    _worker;

    // Basics
    commands::registry _cmd;
    logger_ptr         _logging = share_logger("PERFKIT:NET");

    // RPC connection context
    shared_ptr<terminal_monitor> _rpc_monitor = std::make_shared<terminal_monitor>(this, _logging);
    msgpack::rpc::context        _rpc;

    // Connection
    asio::ip::tcp::acceptor _acceptor{_ioc};

    // Commands that are pending execution
    notify_queue<string> _pending_commands;

    // TTY
    spinlock                      _tty_lock;
    circular_queue<char>          _tty_buf{2 << 20};
    int64_t                       _tty_fence = 0;
    locked<message::tty_output_t> _tty_obuf;

    // States
    shared_ptr<void> _session_active_state_anchor;

    // Misc
    message::service::session_info_t  _session_info;
    message::notify::session_status_t _session_status;
    spinlock                          _session_status_lock;

    // Contexts
    config_context _ctx_config{&_event_proc, &_rpc};

   public:
    explicit terminal(terminal_info info) noexcept;
    void _start_();
    ~terminal();

   private:
    auto CPPH_LOGGER() const { return _logging.get(); }
    void _tick_worker();
    auto _build_service() -> msgpack::rpc::service_info;

    void _on_char_buf(char const*, size_t);

   private:
    void _open_acceptor();
    void _on_session_list_change();

   public:
    optional<string>    fetch_command(milliseconds timeout) override;
    void                push_command(string_view command) override;
    void                write(string_view str) override;
    commands::registry* commands() override { return &_cmd; }
};

}  // namespace perfkit::net
