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

#include "perfkit/common/functional.hxx"
#include "utils.hpp"

perfkit::net::terminal::terminal(perfkit::net::terminal_info info) noexcept
        : _info(std::move(info)), _rpc(_build_service())
{
}

void perfkit::net::terminal::_start_()
{
    detail::input_redirect(bind_front(&terminal::_on_char_buf, this));
}

perfkit::net::terminal::~terminal()
{
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
                   [this](string const& d) {
                       this->push_command(d);
                   })
            .serve(service::fetch_tty,
                   [this](tty_output_t* out, int64_t fence) {

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
    lock_guard _lc_{_tty_lock};
    _tty.rotate_append(data, data + size);
    _tty_fence += size;

    _tty_membuf.content.assign(data, size);
    _tty_membuf.fence = _tty_fence;
    message::notify::tty(_rpc).notify_all(_tty_membuf);
}
