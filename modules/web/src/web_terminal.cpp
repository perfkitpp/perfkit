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

#include "web_terminal.hpp"

#include <cpph/std/filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include <cpph/utility/timer.hxx>
#include <crow/http_response.h>
#include <crow/mustache.h>
#include <crow/websocket.h>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "interface.hpp"
#include "perfkit/detail/backend-utils.hpp"

static auto CPPH_LOGGER()
{
    return perfkit::backend::nglog();
}

namespace perfkit::web::impl {
class terminal::ws_tty_session : public detail::if_websocket_session
{
   public:
    terminal* owner_ = nullptr;

    void on_message(const string& message, bool is_binary) noexcept override
    {
        owner_->push_command(message);
    }
};

terminal::terminal(open_info info) noexcept : info_(std::move(info))
{
    using HTTP = crow::HTTPMethod;
    CPPH_debug("");

    loader_path_buf_ = info_.static_dir;

    auto make_file_load_response =
            [this](auto base, auto&&... args) {
                char uri[512];
                fmt::format_to_n(uri, sizeof uri - 1, base, args...).out[0] = 0;

                crow::response e;
                fs::path path = info_.static_dir;
                path /= uri;

                e.set_static_file_info_unsafe(path.string());
                return e;
            };

    // TODO: App Authentication Middleware

    // Route basic static file publish
    CROW_ROUTE(app_, "/")
    ([=] { return make_file_load_response("index.html"); });

    CROW_ROUTE(app_, "/<string>")
    ([=](string_view path) { return make_file_load_response(path); });

    CROW_ROUTE(app_, "/static/<path>")
    ([=](string_view path) { return make_file_load_response("static/{}", path); });

    // Route configuration APIs / Websockets
    CROW_ROUTE(app_, "/api/test")
    ([] { return "HELL, WORLD!"; });

    // CROW_ROUTE(app_, "/api/config/commit");

    // Route trace APIs

    // CROW_WEBSOCKET_ROUTE(app_, "/ws/trace");
    // CROW_ROUTE(app_, "/api/trace/control");
    // CROW_ROUTE(app_, "/api/trace/select");

    // TTY APIs
    CROW_ROUTE(app_, "/api/tty/commit").methods(HTTP::Post)(bind_front(&terminal::api_tty_commit_, this));

    // Route websockets
    CROW_WEBSOCKET_ROUTE(app_, "/ws/tty")
            .onaccept(bind_front(&terminal::ws_tty_accept_, this))
            .onopen(bind_front(&terminal::ws_on_open_, this))
            .onmessage(bind_front(&terminal::ws_on_msg_, this))
            .onerror(bind_front(&terminal::ws_on_error_, this))
            .onclose(bind_front(&terminal::ws_on_close_, this));

    CROW_WEBSOCKET_ROUTE(app_, "/ws/config")
            .onaccept(bind_front(&terminal::ws_config_accept_, this))
            .onopen(bind_front(&terminal::ws_on_open_, this))
            .onmessage(bind_front(&terminal::ws_on_msg_, this))
            .onerror(bind_front(&terminal::ws_on_error_, this))
            .onclose(bind_front(&terminal::ws_on_close_, this));

    CROW_WEBSOCKET_ROUTE(app_, "/ws/trace")
            .onaccept(bind_front(&terminal::ws_trace_accept_, this))
            .onopen(bind_front(&terminal::ws_on_open_, this))
            .onmessage(bind_front(&terminal::ws_on_msg_, this))
            .onerror(bind_front(&terminal::ws_on_error_, this))
            .onclose(bind_front(&terminal::ws_on_close_, this));

    CROW_WEBSOCKET_ROUTE(app_, "/ws/window")
            .onaccept(bind_front(&terminal::ws_window_accept_, this))
            .onopen(bind_front(&terminal::ws_on_open_, this))
            .onmessage(bind_front(&terminal::ws_on_msg_, this))
            .onerror(bind_front(&terminal::ws_on_error_, this))
            .onclose(bind_front(&terminal::ws_on_close_, this));

    app_.port(info_.bind_port);
    app_.concurrency(1);

    // Redirect input
    CPPH_INFO("* Redirecting terminal output ...");
}

terminal::~terminal()
{
    app_.stop();
    app_worker_.join();
}

void terminal::write(std::string_view str)
{
    auto payload = ioc_.queue().allocate_temporary_payload(str.size());
    copy(str, payload.get());
}

void terminal::I_launch()
{
    app_worker_ = std::thread{[this] { app_.run(); }};
}

void terminal::api_tty_commit_(const crow::request& req, crow::response& rep)
{
    crow::multipart::message msg{req};
    this->push_command(msg.get_part_by_name("command").body);

    rep.end();
}

void terminal::ws_on_close_(crow::websocket::connection& c, const string& why)
{
    CPPH_INFO("* ({}) Closing websocket: {}", c.get_remote_ip(), why);

    auto pp_sess = (detail::websocket_ptr*)c.userdata();
    c.userdata(nullptr);
    pp_sess->get()->on_close(why);

    stopwatch waiting_timer;
    weak_ptr<void> wait_expire = *pp_sess;
    delete pp_sess;

    for (poll_timer tim{3s}; not wait_expire.expired();) {
        if (tim.check_sparse())
            CPPH_WARN("! Waiting for websocket release for {:.1f} seconds ...", waiting_timer.elapsed().count());

        std::this_thread::sleep_for(5ms);
    }

    CPPH_INFO("* WebSocket closed. ({:.3f} seconds)", waiting_timer.elapsed().count());
}

bool terminal::ws_tty_accept_(const crow::request& req, void** ppv)
{
    CPPH_INFO("* Accepting terminal WebSocket from: {}", req.remote_ip_address);
    *ppv = new detail::websocket_ptr{[&, p = make_shared<ws_tty_session>()] { return p->owner_ = this, p; }()};
    return true;
}

void terminal::ws_on_open_(crow::websocket::connection& c)
{
    CPPH_INFO("* Opening WebSocket: {}", c.get_remote_ip());
    auto p = ((detail::websocket_ptr*)c.userdata())->get();
    p->I_register_(&c);
    p->on_open();
}

void terminal::ws_on_error_(crow::websocket::connection& c, const string& what)
{
    CPPH_ERROR("! ({}) WebSocket Error: {}", c.get_remote_ip(), what);
    ((detail::websocket_ptr*)c.userdata())->get()->on_error(what);
}

}  // namespace perfkit::web::impl

auto perfkit::web::open(perfkit::web::open_info info)
        -> std::shared_ptr<perfkit::if_terminal>
{
    auto p_term = make_shared<impl::terminal>(info);
    p_term->I_launch();
    return p_term;
}
