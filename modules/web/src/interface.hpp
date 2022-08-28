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

#pragma once
#include <cpph/std/string>
#include <cpph/std/string_view>

#include <cpph/utility/generic.hxx>

namespace crow::websocket {
struct connection;
}

namespace perfkit::web::detail {
using namespace cpph;

class if_websocket_session : public std::enable_shared_from_this<if_websocket_session>
{
    crow::websocket::connection* conn_ = nullptr;
    string remote_ip_;

   public:
    ~if_websocket_session() noexcept = default;

   public:
    void I_register_(decltype(conn_) conn) noexcept;

   public:
    auto& connection() const noexcept { return *conn_; }
    auto remote_ip() const noexcept -> string const&;

   public:
    virtual void on_open() noexcept {}
    virtual void on_message(string const& message, bool is_binary) noexcept {}
    virtual void on_close(string const& why) noexcept {}
    virtual void on_error(string const& what) noexcept {}

   public:
    void send_binary(string const& content) noexcept;
    void send_text(string const& content) noexcept;
};

using websocket_ptr = shared_ptr<if_websocket_session>;
using websocket_weak_ptr = weak_ptr<if_websocket_session>;

}  // namespace perfkit::web::detail
