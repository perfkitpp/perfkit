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

#include <fmt/core.h>
#include <spdlog/spdlog.h>

#include "crow/http_response.h"
#include "crow/mustache.h"
#include "crow/websocket.h"
#include "fmt/core.h"
#include "perfkit/detail/backend-utils.hpp"

static auto CPPH_LOGGER()
{
    return perfkit::backend::nglog();
}

namespace perfkit::web::impl {
terminal::terminal(open_info info) noexcept : info_(std::move(info))
{
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

    // CROW_WEBSOCKET_ROUTE(app_, "/ws/config");
    // CROW_ROUTE(app_, "/api/config/commit");

    // Route trace APIs

    // CROW_WEBSOCKET_ROUTE(app_, "/ws/trace");
    // CROW_ROUTE(app_, "/api/trace/control");
    // CROW_ROUTE(app_, "/api/trace/select");

    // Route TTY websockets
    CROW_WEBSOCKET_ROUTE(app_, "/ws/tty")
            .onaccept(bind_front(&terminal::accept_ws_, this))
            .onopen(bind_front(&terminal::tty_open_ws_, this))
            .onmessage(bind_front(&terminal::tty_handle_msg_, this))
            .onclose(bind_front(&terminal::close_ws_, this));

    // CROW_ROUTE(app_, "/api/tty/command");
    // CROW_ROUTE(app_, "/api/tty/suggest");

    app_.port(info_.bind_port);
    app_.concurrency(1);

    // Redirect input
}

terminal::~terminal()
{
    app_.stop();
    app_worker_.join();
}

void terminal::write(std::string_view str)
{
}

void terminal::I_launch()
{
    app_worker_ = std::thread{[this] { app_.run(); }};
}

void terminal::tty_open_ws_(crow::websocket::connection& conn)
{
    CPPH_debug("* Client '{}' opened TTY socket", conn.get_remote_ip());
    conn.send_text("HELL WOLRD WEBSOCKET!\n");
}

void terminal::close_ws_(crow::websocket::connection& conn, const string& why)
{
    CPPH_debug("* Client '{}' closing socket for: {}", conn.get_remote_ip(), why);
}

bool terminal::accept_ws_(crow::request const& req, void**)
{
    CPPH_debug("* Client '{}' requested for websocket connection", req.remote_ip_address);
    return true;
}
}  // namespace perfkit::web::impl

auto perfkit::web::open(perfkit::web::open_info info)
        -> std::shared_ptr<perfkit::if_terminal>
{
    auto p_term = make_shared<impl::terminal>(info);
    p_term->I_launch();
    return p_term;
}
