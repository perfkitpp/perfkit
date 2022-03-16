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
#include "utils.hpp"

using namespace std::literals;

perfkit::net::terminal::terminal(perfkit::net::terminal_info info) noexcept
        : _info(std::move(info)), _rpc(_build_service(), _rpc_monitor)
{
}

void perfkit::net::terminal::_start_()
{
    detail::input_redirect(bind_front(&terminal::_on_char_buf, this));

    CPPH_INFO("Starting session [{}] ...", _info.name);
    _worker.repeat(bind_front(&terminal::_worker_func, this));
}

void perfkit::net::terminal::_worker_func()
{
    try
    {
        // If listening socket is not initialized yet, do initialize.
        if (not _acceptor.is_open())
        {
            using namespace asio::ip;
            tcp::endpoint ep{make_address(_info.bind_ip), _info.bind_port};
            CPPH_INFO("accept>> Binding acceptor to {}:{} ...", _info.bind_ip, _info.bind_port);

            _acceptor.open(ep.protocol());
            _acceptor.set_option(asio::socket_base::reuse_address{true});
            _acceptor.bind(ep);
            CPPH_INFO("accept>> Bind successful. Starting listening ...");

            msgpack::rpc::session_config config;
            config.timeout         = 10s;
            config.use_integer_key = true;

            msgpack::rpc::asio_ex::open_acceptor(_rpc, config, _acceptor);
            CPPH_INFO("accept>> Now listening ...");
        }

        std::this_thread::sleep_for(100ms);

        // TODO:
    }
    catch (asio::system_error& ec)
    {
        CPPH_ERROR("Socket error! ({}): {}", ec.code().value(), ec.what());
        CPPH_ERROR("Sleep for 3 seconds before retry ...");

        std::this_thread::sleep_for(3s);
    }
}

perfkit::net::terminal::~terminal()
{
    // Send stop signal to worker, and close acceptor not to receive foreign connections
    _worker.stop();
    _acceptor.close();

    // Disconnect all remote clients
    _rpc.disconnect_all();

    // Wait all async
    _worker.join();
    _ioc.join();

    detail::input_rollback();
}

perfkit::msgpack::rpc::service_info perfkit::net::terminal::_build_service()
{
    using namespace message;

    msgpack::rpc::service_info service_desc{};
    service_desc
            .serve(service::suggest,
                   [this](service::suggest_result_t*, string const& content, int cursor) {
                       // TODO
                   })
            .serve(service::invoke_command,
                   [this](auto&&, string const& content) {
                       this->push_command(content);
                   })
            .serve(service::fetch_tty,
                   [this](tty_output_t* out, int64_t fence) {
                       // Returns requested range of bytes
                       lock_guard _lc_{_tty_lock};
                       out->fence = _tty_fence;

                       fence      = std::clamp<int64_t>(fence, _tty_fence - _tty_buf.size(), _tty_fence);
                       auto begin = _tty_buf.end() - (_tty_fence - fence);
                       auto end   = _tty_buf.end();

                       out->content.assign(begin, end);
                   })
            .serve(service::heartbeat,
                   [this] { CPPH_INFO("HEARTBEAT!"); });

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

    _tty_obuf.use([&](message::tty_output_t& buf) {
        buf.content.assign(data, size);
        buf.fence = _tty_fence;
        message::notify::tty(_rpc).notify_all(buf);
    });
}

void perfkit::net::terminal_monitor::on_new_session(
        const perfkit::msgpack::rpc::session_profile& profile) noexcept
{
    CPPH_INFO("session [{}]>> Connected", profile.peer_name);
}

void perfkit::net::terminal_monitor::on_dispose_session(
        const perfkit::msgpack::rpc::session_profile& profile) noexcept
{
    CPPH_INFO("session [{}]>> Disconnecting ... ", profile.peer_name);
}
