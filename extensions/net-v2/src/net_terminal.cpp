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

#include "net_terminal.hpp"

#include <spdlog/spdlog.h>

#include "perfkit/common/functional.hxx"
#include "perfkit/common/refl/msgpack-rpc/asio.hxx"
#include "perfkit/configs.h"
#include "utils.hpp"

using namespace std::literals;

perfkit::net::terminal::terminal(perfkit::net::terminal_info info) noexcept
        : _info(std::move(info)),
          _rpc(
                  _build_service(),
                  [this](auto&& fn) { asio::post(_thread_pool, std::forward<decltype(fn)>(fn)); },
                  _rpc_monitor)
{
    using namespace std::chrono;
    _session_info.epoch = duration_cast<milliseconds>(
                                  steady_clock::now().time_since_epoch())
                                  .count();
    _session_info.name = _info.name;
    _session_info.description = _info.description;
    _session_info.num_cores = std::thread::hardware_concurrency();

    // key string generation
    char hostname[65];
    gethostname(hostname, std::size(hostname));

    _session_info.hostname = hostname;
    _session_info.keystr
            = _session_info.hostname + ';'
            + std::to_string(_session_info.num_cores) + ';'
            + _session_info.name + ';'
            + std::to_string(_session_info.epoch);
}

void perfkit::net::terminal::_start_()
{
    detail::input_redirect(bind_front(&terminal::_on_char_buf, this));

    CPPH_INFO("Starting session [{}] ...", _info.name);
    _worker.repeat(bind_front(&terminal::_tick_worker, this));

    // Post event for opening acceptor
    _event_proc.post(bind_front(&terminal::_open_acceptor, this));

    // Start stat() system
    _publish_system_stat(asio::error_code{});
}

void perfkit::net::terminal::_open_acceptor()
{
    using namespace asio::ip;
    tcp::endpoint ep{make_address(_info.bind_ip), _info.bind_port};
    CPPH_INFO("accept>> Binding acceptor to {}:{} ...", _info.bind_ip, _info.bind_port);

    _acceptor.open(ep.protocol());
    _acceptor.set_option(asio::socket_base::reuse_address{true});
    _acceptor.bind(ep);
    CPPH_INFO("accept>> Bind successful. Starting listening ...");

    msgpack::rpc::session_config config;
    config.timeout = 10s;
    config.use_integer_key = true;

    msgpack::rpc::asio_ex::open_acceptor(_rpc, config, _acceptor);
    CPPH_INFO("accept>> Now listening ...");
}

void perfkit::net::terminal::_tick_worker()
{
    try {
        _event_proc.restart();
        _event_proc.run_for(2500ms);
    } catch (asio::system_error& ec) {
        CPPH_ERROR("Socket error! ({}): {}", ec.code().value(), ec.what());
        CPPH_ERROR("Sleep for 3 seconds before retry ...");

        std::this_thread::sleep_for(3s);
        _event_proc.post(bind_front(&terminal::_open_acceptor, this));
    }
}

perfkit::net::terminal::~terminal()
{
    // Send stop signal to worker, and close acceptor not to receive foreign connections
    _worker.stop();
    _acceptor.close();

    // Disconnect all remote clients
    _rpc.disconnect_all();

    // Stop
    _event_proc_guard = {};

    // Wait all async
    _worker.join();
    _thread_pool.join();

    detail::input_rollback();
}

perfkit::msgpack::rpc::service_info perfkit::net::terminal::_build_service()
{
    using namespace message;

    msgpack::rpc::service_info service_desc{};
    service_desc
            .route(service::suggest, bind_front(&self_t::_rpc_handle_suggest, this))
            .route(service::invoke_command, bind_front(&self_t::_rpc_handle_command, this))
            .route(service::login, bind_front(&self_t::_rpc_handle_login, this))
            .route(service::heartbeat, [] {})  // do nothing
            .route(service::fetch_tty,
                   [this](tty_output_t* out, int64_t fence) {
                       // Returns requested range of bytes
                       lock_guard _lc_{_tty_lock};
                       out->fence = _tty_fence;

                       fence = std::clamp<int64_t>(fence, _tty_fence - _tty_buf.size(), _tty_fence);
                       auto begin = _tty_buf.end() - (_tty_fence - fence);
                       auto end = _tty_buf.end();

                       out->content.assign(begin, end);
                   })
            .route(service::session_info,
                   [this](perfkit::net::message::service::session_info_t* rv) {
                       *rv = _session_info;
                   })
            .route(service::update_config_entity,
                   [this](auto&& prof, auto&&, auto&& content) {
                       _verify_admin_access(prof);
                       _ctx_config.rpc_update_request(content);
                   })
            .route(service::request_republish_config_registries,
                   [this](auto&& prof, auto&&) {
                       _verify_basic_access(prof);
                       _ctx_config.rpc_republish_all_registries();
                   });

    return service_desc;
}

std::optional<std::string> perfkit::net::terminal::fetch_command(std::chrono::milliseconds timeout)
{
    return _pending_commands.try_pop(timeout);
}

void perfkit::net::terminal::push_command(std::string_view command)
{
    _pending_commands.emplace(command);
}

void perfkit::net::terminal::write(std::string_view str)
{
    detail::write(str.data(), str.size());
}

void perfkit::net::terminal::_on_char_buf(const char* data, size_t size)
{
    {
        lock_guard _lc_{_tty_lock};
        _tty_buf.rotate_append(data, data + size);
        _tty_fence += size;
    }

    _tty_obuf.access([&](message::tty_output_t& buf) {
        buf.content.assign(data, size);
        buf.fence = _tty_fence;
        message::notify::tty(_rpc).notify_all(buf);
    });
}

void perfkit::net::terminal::_on_session_list_change()
{
    auto const num_sessions = _rpc.session_count();
    CPPH_INFO("{} sessions are currently active.", num_sessions);

    bool const should_start_monitoring = num_sessions;
    bool const has_no_change = should_start_monitoring == !!_session_active_state_anchor;

    if (has_no_change)
        return;

    if (should_start_monitoring) {
        CPPH_INFO("First session created ... Starting monitoring.");

        // Start monitoring
        _session_active_state_anchor = std::make_shared<nullptr_t>();
        weak_ptr anchor = _session_active_state_anchor;

        _ctx_config.start_monitoring(anchor);
    } else {
        CPPH_INFO("Last session disposed ... Stopping monitoring.");
        _ctx_config.stop_monitoring();

        _session_active_state_anchor = nullptr;
    }
}

bool perfkit::net::terminal::_has_admin_access(
        const perfkit::msgpack::rpc::session_profile& profile) const
{
    if (auto ptr = find_ptr(*_verified_sessions.lock(), &profile))
        return ptr->second.has_admin_access;
    else
        return false;
}

void perfkit::net::terminal::_rpc_handle_login(
        const perfkit::msgpack::rpc::session_profile& profile, message::auth_level_t* b, std::string& auth)
{
    // TODO: Implement ID/PW authentication logic!
    {
        CPPH_INFO("session {} requested login ... always given admin right!!", profile.peer_name);

        _verified_sessions.access([&](decltype(_verified_sessions)::value_type& ref) {
            ref[&profile].has_admin_access = true;
        });

        *b = message::auth_level_t::admin_access;
    }

    // zero-fill auth info
    std::fill(auth.begin(), auth.end(), 0);
}

void perfkit::net::terminal::_rpc_handle_command(
        session_profile const& profile, void*,
        const std::string&     content)
{
    _verify_admin_access(profile);
    this->push_command(content);
}

void perfkit::net::terminal::_verify_admin_access(const perfkit::msgpack::rpc::session_profile& profile) const
{
    if (not _has_admin_access(profile))
        throw std::runtime_error("Non-admin peer: " + profile.peer_name);
}

void perfkit::net::terminal::_verify_basic_access(const perfkit::msgpack::rpc::session_profile& profile) const
{
    if (not _has_basic_access(profile))
        throw std::runtime_error("Non-authorized peer: " + profile.peer_name);
}

void perfkit::net::terminal::_publish_system_stat(asio::error_code ec)
{
    if (ec) { return; }

    if (detail::proc_stat_t stat = {}; _session_active_state_anchor && detail::fetch_proc_stat(&stat)) {
        size_t in, out;
        _rpc.totals(&in, &out);
        auto delta_in = in - _session_prev_bytes[0];
        auto delta_out = out - _session_prev_bytes[1];
        auto dt = _session_state_delta_timer.elapsed().count();
        int  in_rate = delta_in / dt;
        int  out_rate = delta_out / dt;
        _session_state_delta_timer.reset();
        _session_prev_bytes[0] = in;
        _session_prev_bytes[1] = out;

        message::notify::session_status_t message
                = {stat.cpu_usage_total_user,
                   stat.cpu_usage_total_system,
                   stat.cpu_usage_self_user,
                   stat.cpu_usage_self_system,
                   stat.memory_usage_virtual,
                   stat.memory_usage_resident,
                   stat.num_threads,
                   out_rate, in_rate};

        message::notify::session_status(_rpc).notify_all(message, _fn_basic_access());
    }

    _session_stat_timer.expires_after(2500ms);
    _session_stat_timer.async_wait(bind_front(&self_t::_publish_system_stat, this));
}

void perfkit::net::terminal_monitor::on_new_session(
        const perfkit::msgpack::rpc::session_profile& profile) noexcept
{
    CPPH_INFO("session [{}]>> Connected", profile.peer_name);
    asio::post(_owner->_event_proc, bind_front(&terminal::_on_session_list_change, _owner));
}

void perfkit::net::terminal_monitor::on_dispose_session(
        const perfkit::msgpack::rpc::session_profile& profile) noexcept
{
    if (_owner->_verified_sessions.lock()->erase(&profile))
        CPPH_INFO("Authorized session [{}]>> Disconnecting ... ", profile.peer_name);
    else
        CPPH_INFO("Unauthorized session [{}]>> Disconnecting ... ", profile.peer_name);

    asio::post(_owner->_event_proc, bind_front(&terminal::_on_session_list_change, _owner));
}
