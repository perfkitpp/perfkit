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

#include "cpph/functional.hxx"
#include "cpph/refl/object.hxx"
#include "cpph/refl/rpc/connection/asio.hxx"
#include "cpph/refl/rpc/protocol/msgpack-rpc.hxx"
#include "cpph/refl/rpc/rpc.hxx"
#include "cpph/refl/rpc/service.hxx"
#include "cpph/refl/rpc/session_builder.hxx"
#include "cpph/template_utils.hxx"
#include "perfkit/configs.h"
#include "utils.hpp"

using namespace std::literals;

perfkit::net::terminal::terminal(perfkit::net::terminal_info info) noexcept
        : _info(std::move(info)),
          _rpc_service(_build_service())
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
    _rpc_monitor = std::make_shared<terminal_monitor>(std::static_pointer_cast<terminal>(shared_from_this()), _logging);

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
    if (_acceptor.is_open()) { _acceptor.close(); }

    using namespace asio::ip;
    tcp::endpoint ep{make_address(_info.bind_ip), _info.bind_port};
    CPPH_INFO("accept>> Binding acceptor to {}:{} ...", _info.bind_ip, _info.bind_port);

    _acceptor.open(ep.protocol());
    _acceptor.set_option(asio::socket_base::reuse_address{true});
    _acceptor.bind(ep);
    ep = _acceptor.local_endpoint();
    CPPH_INFO("accept>> Binding successful. Starting listening on {}:{} ...", _info.bind_ip, ep.port());
    _acceptor.listen();

    // Open find_me socket
    if (_info.enable_find_me and not _findme_socket.is_open()) {
        try {
            using asio::ip::udp;
            _findme_socket.open(udp::v4());

            _findme_socket.set_option(udp::socket::reuse_address{true});
            _findme_socket.set_option(udp::socket::broadcast{true});
        } catch (asio::system_error& e) {
            CPPH_INFO("findme>> Socket configuration failed: ({}) {}", e.code().value(), e.what());
            asio::error_code ec;
            _findme_socket.close(ec);
        }
    }

    {
        // Cache find_me payload
        message::find_me_t find_me;
        find_me.alias = _info.name;
        find_me.port = _acceptor.local_endpoint().port();

        streambuf::stringbuf buf{&_findme_payload};
        archive::json::writer writer{&buf};
        writer << find_me;
    }

    // Implement accept logic
    auto fn_acpt = y_combinator{
            [this](auto&& self, auto&& ec) {
                if (ec) { throw asio::system_error(ec); }

                {
                    auto ep = _accept_socket.remote_endpoint();
                    CPPH_INFO("accept>> Accepting new connection {}:{} ...",
                              ep.address().to_string(), ep.port());
                }

                auto socket = exchange(_accept_socket, tcp::socket{_thread_pool});
                auto session = rpc::session_ptr{};

                rpc::session::builder{}
                        .protocol(make_unique<rpc::protocol::msgpack>())
                        .connection(make_unique<rpc::asio_stream<tcp>>(std::move(socket)))
                        .event_procedure(_sess_evt_proc)
                        .service(_rpc_service)
                        .monitor(_rpc_monitor)
                        .build_to(session);

                _rpc.add_session(session);
                _acceptor.async_accept(_accept_socket, self);
            }};

    _acceptor.async_accept(_accept_socket, fn_acpt);

    CPPH_INFO("accept>> Now socket is listening ...");
}

void perfkit::net::terminal::_tick_worker()
{
    try {
        _event_proc.restart();
        _event_proc.run_for(2500ms);
    } catch (asio::system_error& ec) {
        if (ec.code().value() == asio::error::operation_aborted) { return; }
        if (ec.code().value() == asio::error::connection_aborted) { return; }

        CPPH_ERROR("System error! ({}): {}", ec.code().value(), ec.what());
        CPPH_ERROR("Sleep for 3 seconds before restart ...");

        std::this_thread::sleep_for(3s);

        if (not _acceptor.is_open()) {
            _event_proc.post(bind_front(&terminal::_open_acceptor, this));
        }
    }
}

perfkit::net::terminal::~terminal()
{
    // Send stop signal to worker, and close acceptor not to receive foreign connections
    _worker.stop();
    _acceptor.close();

    // Disconnect all remote clients
    for (auto& s : _rpc.release()) { s->close(); }

    // Stop
    _event_proc_guard = {};

    // Wait all async operations
    _worker.join();

    // Stop thread pool
    _thread_pool.stop();
    _thread_pool.join();

    detail::input_rollback();
}

auto perfkit::net::terminal::_build_service() -> rpc::service
{
    using namespace message;

    auto builder = rpc::service::builder{};
    builder
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
            .route(service::request_republish_registries,
                   [this](auto&& prof, auto&&) {
                       _verify_basic_access(prof);
                       _ctx_config.rpc_republish_all_registries();
                       _ctx_trace.rpc_republish_all_tracers();
                   });

    _ctx_trace.build_service(builder);
    return builder.build();
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
        _tty_buf.enqueue_n(data, size);
        _tty_fence += size;
    }

    _tty_obuf.access([&](message::tty_output_t& buf) {
        buf.content.assign(data, size);
        buf.fence = _tty_fence;
        message::notify::tty(&_rpc).notify(buf);
    });
}

void perfkit::net::terminal::_on_session_list_change()
{
    auto const num_sessions = (_rpc.gc(), _rpc.size());
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
        _ctx_trace.start_monitoring(anchor);
    } else {
        CPPH_INFO("Last session disposed ... Stopping monitoring.");
        _ctx_config.stop_monitoring();
        _ctx_trace.stop_monitoring();

        _session_active_state_anchor = nullptr;
    }
}

bool perfkit::net::terminal::_has_admin_access(session_profile_view profile) const
{
    if (auto ptr = find_ptr(*_verified_sessions.lock(), profile))
        return ptr->second.has_admin_access;
    else
        return false;
}

void perfkit::net::terminal::_rpc_handle_login(
        session_profile_view profile, message::auth_level_t* b, std::string& auth)
{
    // TODO: Implement ID/PW authentication logic!
    {
        CPPH_INFO("session {} requested login ... always given admin right!!", profile->peer_name);

        _verified_sessions.access([&](decltype(_verified_sessions)::value_type& ref) {
            ref[profile].has_admin_access = true;
        });

        *b = message::auth_level_t::admin_access;
    }

    // zero-fill auth info
    std::fill(auth.begin(), auth.end(), 0);
}

void perfkit::net::terminal::_rpc_handle_command(
        session_profile_view profile, void*, const std::string& content)
{
    _verify_admin_access(profile);
    this->push_command(content);
}

void perfkit::net::terminal::_verify_admin_access(session_profile_view profile) const
{
    if (not _has_admin_access(profile))
        throw std::runtime_error("Non-admin peer: "s.append(profile->peer_name));
}

void perfkit::net::terminal::_verify_basic_access(session_profile_view profile) const
{
    if (not _has_basic_access(profile))
        throw std::runtime_error("Non-authorized peer: "s.append(profile->peer_name));
}

void perfkit::net::terminal::_publish_system_stat(asio::error_code ec)
{
    if (ec) { return; }

    // Publish status changes
    if (detail::proc_stat_t stat = {}; _session_active_state_anchor && detail::fetch_proc_stat(&stat)) {
        size_t in, out;
        _rpc.totals(&in, &out);
        auto delta_in = in - _session_prev_bytes[0];
        auto delta_out = out - _session_prev_bytes[1];
        auto dt = _session_state_delta_timer.elapsed().count();
        int in_rate = delta_in / dt;
        int out_rate = delta_out / dt;
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

        message::notify::session_status(&_rpc).notify(message, _fn_basic_access());
    }

    // Publish find_me packet
    if (_findme_socket.is_open()) {
        auto ep = asio::ip::udp::endpoint{asio::ip::address_v4::broadcast(), message::find_me_port};
        _findme_socket.send_to(asio::buffer(_findme_payload), ep, {}, ec);

        if (ec) {
            CPPH_ERROR("Failed to publish FINDME packet: ({}) {}", ec.value(), ec.message());
        }
    }

    _session_stat_timer.expires_after(2500ms);
    _session_stat_timer.async_wait(bind_front(&self_t::_publish_system_stat, this));
}

void perfkit::net::terminal_monitor::on_session_created(session_profile_view profile) noexcept
{
    CPPH_INFO("session [{}]>> Connected", profile->peer_name);
    if (auto lc = _owner.lock())
        asio::post(lc->_event_proc, bind_front(&terminal::_on_session_list_change, lc.get()));
}

void perfkit::net::terminal_monitor::on_session_expired(session_profile_view profile) noexcept
{
    if (auto lc = _owner.lock()) {
        if (lc->_verified_sessions.lock()->erase(profile))
            CPPH_INFO("Authorized session [{}]>> Disconnecting ... ", profile->peer_name);
        else
            CPPH_INFO("Unauthorized session [{}]>> Disconnecting ... ", profile->peer_name);

        asio::post(lc->_event_proc, bind_front(&terminal::_on_session_list_change, lc.get()));
    }
}

void perfkit::net::terminal::session_event_procedure_t::post_rpc_completion(perfkit::function<void()>&& fn)
{
    assert(false && "This may not be called as terminal never use RPC feature!");
}

void perfkit::net::terminal::session_event_procedure_t::post_handler_callback(perfkit::function<void()>&& fn)
{
    asio::post(_owner->_event_proc, std::move(fn));
}

void perfkit::net::terminal::session_event_procedure_t::post_internal_message(perfkit::function<void()>&& fn)
{
    asio::post(_owner->_thread_pool, std::move(fn));
}
