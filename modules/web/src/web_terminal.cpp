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

#include <fstream>
#include <sstream>
#include <utility>

#include "crow/http_response.h"
#include "crow/mustache.h"

namespace perfkit::web::impl {
terminal::terminal(open_info info) noexcept : info_(std::move(info))
{
    loader_path_buf_ = info_.static_dir;

    auto fn_index_html = [this] {
        if (auto p_index = _web_load_mustache("/index.html")) {
            crow::mustache::context ctx;
            ctx["TITLE"] = info_.alias;

            return p_index->render(ctx).dump();
        } else {
            return string{"404"};
        }
    };

    CROW_ROUTE(app_, "/")
    (fn_index_html);
    CROW_ROUTE(app_, "/<path>")
    ([this](string_view path) {
        crow::response e;
        if (auto p_content = _web_load_file("/"s.append(path))) {
            e.body = *p_content;
            e.code = crow::OK;
        }

        return e;
    });

    app_.port(info_.bind_port);
    app_.concurrency(1);
}

void terminal::write(std::string_view str)
{
}

auto terminal::_web_load_mustache(string_view path) -> shared_ptr<crow::mustache::template_t>
{
    // TODO: Cache mode ... prevent recompiling

    if (auto str = _web_load_file(path)) {
        return make_shared<crow::mustache::template_t>(crow::mustache::compile(*str));
    }

    return nullptr;
}

auto terminal::_web_load_file(string_view path) -> shared_ptr<string>
{
    loader_path_buf_.resize(info_.static_dir.size());
    loader_path_buf_.append(path);

    // TODO: Cache mode ... prevent reloading

    if (std::ifstream ifs{loader_path_buf_, std::ios::binary}; ifs.is_open()) {
        std::stringstream ss;
        ss << ifs.rdbuf();

        return make_shared<string>(ss.str());
    }

    return nullptr;
}

}  // namespace perfkit::web::impl

auto perfkit::web::open(perfkit::web::open_info info)
        -> std::shared_ptr<perfkit::if_terminal>
{
    auto p_term = make_shared<impl::terminal>(info);
    p_term->I_launch();
    return p_term;
}
