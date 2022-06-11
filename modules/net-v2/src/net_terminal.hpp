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
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <asio/thread_pool.hpp>

#include "context/config_context.hpp"
#include "context/trace_context.hpp"
#include "cpph/container/circular_queue.hxx"
#include "cpph/refl/rpc/rpc.hxx"
#include "cpph/refl/rpc/service.hxx"
#include "cpph/thread/locked.hxx"
#include "cpph/thread/notify_queue.hxx"
#include "cpph/thread/worker.hxx"
#include "cpph/utility/hasher.hxx"
#include "cpph/utility/timer.hxx"
#include "net_terminal_adapter.hpp"
#include "perfkit/detail/commands.hpp"
#include "perfkit/extension/net/protocol.hpp"
#include "perfkit/logging.h"
#include "perfkit/terminal.h"

namespace perfkit::net {
using rpc::session_profile;
using session_profile_view = rpc::session_profile_view;
using std::optional;
using std::shared_ptr;
using std::string;
using std::string_view;
using std::vector;
using std::weak_ptr;
using std::chrono::steady_clock;

struct terminal_info {
    string name;
    string description;

    string bind_ip;
    uint16_t bind_port;

    // TODO: Authentication
    bool enable_find_me;
};

class terminal;

class terminal_monitor : public rpc::if_session_monitor
{
    weak_ptr<terminal> _owner;
    perfkit::logger_ptr _logger;

   public:
    explicit terminal_monitor(weak_ptr<terminal> owner, logger_ptr logger)
            : _owner(std::move(owner)), _logger(std::move(logger)) {}

   public:
    void on_session_expired(session_profile_view view) noexcept override;
    void on_session_created(session_profile_view view) noexcept override;

   private:
    auto CPPH_LOGGER() const { return &*_logger; }
};

struct terminal_session_descriptor {
    bool has_admin_access = false;
};

class terminal : public if_terminal
{
    using self_t = terminal;
    friend class terminal_monitor;

   private:
    class adapter_t : public if_net_terminal_adapter
    {
        terminal* _owner;

       public:
        explicit adapter_t(terminal* owner) noexcept : _owner(owner) {}

       public:
        bool has_basic_access(const session_profile* profile) const override { return _owner->_has_basic_access(profile); }
        bool has_admin_access(const session_profile* profile) const override { return _owner->_has_admin_access(profile); }
        asio::io_context* event_proc() override { return &_owner->_event_proc; }
        rpc::session_group* rpc() override { return &_owner->_rpc; }
    };

    class session_event_procedure_t : public rpc::if_event_proc
    {
        terminal* _owner;

       public:
        explicit session_event_procedure_t(terminal* owner) : _owner(owner) {}

       public:
        void post_rpc_completion(function<void()>&& fn) override;
        void post_handler_callback(function<void()>&& fn) override;
        void post_internal_message(function<void()>&& fn) override;
    };

   private:
    adapter_t _adapter{this};
    terminal_info _info;

    // Thread pool
    asio::io_context _event_proc;
    asio::any_io_executor _event_proc_guard = asio::require(_event_proc.get_executor(), asio::execution::outstanding_work_t::tracked);
    asio::thread_pool _thread_pool{4};
    thread::worker _worker;

    // Basics
    logger_ptr _logging = share_logger("PERFKIT:NET");

    // RPC connection context
    shared_ptr<terminal_monitor> _rpc_monitor;
    rpc::session_group _rpc;
    rpc::service _rpc_service;

    // Connection
    asio::ip::tcp::acceptor _acceptor{_event_proc};
    asio::ip::tcp::socket _accept_socket{_thread_pool};
    asio::ip::udp::socket _findme_socket{_thread_pool};
    string _findme_payload;

    //
    using session_event_procedure_ptr = shared_ptr<session_event_procedure_t>;
    session_event_procedure_ptr _sess_evt_proc = make_shared<session_event_procedure_t>(this);

    // Commands that are pending execution
    notify_queue<string> _pending_commands;

    // TTY
    spinlock _tty_lock;
    circular_queue<char> _tty_buf{2 << 20};
    int64_t _tty_fence = 0;
    locked<message::tty_output_t> _tty_obuf;

    // Sessions
    using session_table_t = std::unordered_map<session_profile_view, terminal_session_descriptor>;
    locked<session_table_t> _verified_sessions;

    // States
    shared_ptr<void> _session_active_state_anchor;
    size_t _session_prev_bytes[2] = {};
    stopwatch _session_state_delta_timer = {};

    // Misc
    message::service::session_info_t _session_info;
    asio::steady_timer _session_stat_timer{_thread_pool};

    // Contexts
    config_context _ctx_config{&_adapter};
    trace_context _ctx_trace{&_adapter};

   public:
    explicit terminal(terminal_info info) noexcept;
    void _start_();
    ~terminal() override;

   private:
    auto CPPH_LOGGER() const { return _logging.get(); }
    void _tick_worker();
    auto _build_service() -> rpc::service;

    void _on_char_buf(char const*, size_t);

   private:
    void _open_acceptor();
    void _on_session_list_change();

    void _rpc_handle_login(session_profile_view profile, message::auth_level_t* b, string&);
    void _rpc_handle_suggest(session_profile_view profile, message::service::suggest_result_t*, string const& content, int cursor) {}
    void _rpc_handle_command(session_profile_view profile, void*, string const& content);

    bool _has_basic_access(session_profile_view profile) const { return contains(*_verified_sessions.lock(), profile); }
    bool _has_admin_access(session_profile_view profile) const;

    auto _fn_basic_access() const { return bind_front(&self_t::_has_basic_access, this); }
    auto _fn_admin_access() const { return bind_front(&self_t::_has_admin_access, this); }

    void _verify_admin_access(session_profile_view profile) const;
    void _verify_basic_access(session_profile_view profile) const;

    void _publish_system_stat(asio::error_code ec);

   public:
    optional<string> fetch_command(milliseconds timeout) override;
    void push_command(string_view command) override;
    void write(string_view str) override;
};

}  // namespace perfkit::net
