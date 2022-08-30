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
// Created by ki608 on 2022-08-21.
//

#pragma once
#include "crow/websocket.h"
#define CROW_DISABLE_STATIC_DIR

#include <cpph/container/circular_queue.hxx>
#include <cpph/memory/ring_allocator.hxx>
#include <cpph/thread/locked.hxx>
#include <cpph/thread/notify_queue.hxx>
#include <cpph/thread/thread_pool.hxx>
#include <crow.h>

#include "config_server.hpp"
#include "interface.hpp"
#include "perfkit/terminal.h"
#include "perfkit/web.h"

namespace perfkit::web::impl {
struct basic_socket_context {
    virtual ~basic_socket_context() = default;
};

class terminal : public if_terminal, public detail::if_web_terminal
{
    open_info info_;

    crow::App<> app_;
    std::thread app_worker_;

    notify_queue<string> commands_;
    string loader_path_buf_;

    event_queue_worker ioc_{thread::lazy, 64 << 10};

    vector<detail::websocket_weak_ptr> tty_sockets_;
    circular_queue<char> tty_content_{64 << 10};
    string tty_tmp_shelf_;

    // Sub-services
    ptr<detail::config_server> srv_config_ = detail::config_server::create(this);

   private:
    class ws_tty_session;

   public:
    explicit terminal(open_info) noexcept;
    ~terminal();
    void I_launch();

   public:
    void push_command(std::string_view command) override { commands_.push(string{command}); }
    void write(std::string_view str) override;
    auto event_queue() noexcept -> cpph::event_queue& override { return ioc_.queue(); }

   protected:
    std::optional<std::string> fetch_command(milliseconds timeout) override { return commands_.try_pop(timeout); }

   private:  // APIs
    void api_tty_commit_(crow::request const&, crow::response&);

   private:  // WebSocket Handling
    void ws_on_open_(crow::websocket::connection& c);
    void ws_on_msg_(crow::websocket::connection& c, string const& data, bool is_bin) { ((detail::websocket_ptr*)c.userdata())->get()->on_message(data, is_bin); }
    void ws_on_error_(crow::websocket::connection& c, string const& what);
    void ws_on_close_(crow::websocket::connection& c, string const& why);

    bool ws_tty_accept_(crow::request const&, void**);
    bool ws_config_accept_(crow::request const&, void**);
    bool ws_trace_accept_(crow::request const&, void**) { return false; }
    bool ws_window_accept_(crow::request const&, void**) { return false; }
};
}  // namespace perfkit::web::impl
