/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2021. Seungwoo Kang
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
// Created by ki608 on 2021-11-21.
//

#include "dispatcher.hpp"

#include <utility>

#include <asio/dispatch.hpp>
#include <asio/post.hpp>
#include <perfkit/common/assert.hxx>

#include "basic_dispatcher_impl.hpp"
#include "server_mode_dispatcher.hpp"

perfkit::terminal::net::dispatcher::~dispatcher() = default;
perfkit::terminal::net::dispatcher::dispatcher(
        const perfkit::terminal::net::terminal_init_info& init_info)
{
    // TODO: add advanced options for terminal_init_info
    detail::basic_dispatcher_impl_init_info init0;
    init0.auth = init_info.auths;

    if (init_info._mode == operation_mode::independent_server)
    {
        detail::server_mode_dispatcher_init_info init1;
        init1.bind_port = init_info._port_param;
        init1.bind_addr = init_info._string_param;

        self = std::make_unique<detail::server_mode_dispatcher>(init0, init1);
    }
    else if (init_info._mode == operation_mode::relay_server_provider)
    {
        UNIMPLEMENTED();
    }
    else
    {
        throw std::logic_error{"specify valid terminal operation mode!"};
    }
}

void perfkit::terminal::net::dispatcher::_register_recv(
        std::string route,
        std::function<bool(const recv_archive_type&)> fn)
{
    self->register_recv(std::move(route), std::move(fn));
}

void perfkit::terminal::net::dispatcher::_send(
        std::string_view route,
        int64_t fence,
        void const* userobj,
        void (*payload)(send_archive_type*, void const*))
{
    self->send(route, fence, userobj, payload);
}

perfkit::event<>& perfkit::terminal::net::dispatcher::on_no_connection()
{
    return self->on_no_connection;
}

perfkit::event<int>& perfkit::terminal::net::dispatcher::on_new_connection()
{
    return self->on_new_connection;
}

std::pair<size_t, size_t> perfkit::terminal::net::dispatcher::num_bytes_in_out() const noexcept
{
    return std::make_pair(self->bytes_in(), self->bytes_out());
}

void perfkit::terminal::net::dispatcher::launch()
{
    self->launch();
}

void perfkit::terminal::net::dispatcher::dispatch(perfkit::function<void()> fn)
{
    asio::dispatch(*self->io(), std::move(fn));
}

void perfkit::terminal::net::dispatcher::post(perfkit::function<void()> fn)
{
    asio::post(*self->io(), std::move(fn));
}
