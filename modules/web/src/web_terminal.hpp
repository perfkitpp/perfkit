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
#define CROW_DISABLE_STATIC_DIR
#include <cpph/thread/notify_queue.hxx>
#include <crow.h>

#include "perfkit/terminal.h"
#include "perfkit/web.h"

namespace perfkit::web::impl {
class terminal : public if_terminal
{
    open_info info_;

    crow::App<> app_;
    std::thread app_worker_;

    notify_queue<string> commands_;
    string loader_path_buf_;

   public:
    explicit terminal(open_info) noexcept;
    ~terminal();
    void I_launch();

   public:
    void push_command(std::string_view command) override { commands_.push(string{command}); }
    void write(std::string_view str) override;

   protected:
    std::optional<std::string> fetch_command(milliseconds timeout) override { return commands_.try_pop(timeout); }

   private:
    auto _web_load_mustache(string_view path) -> shared_ptr<crow::mustache::template_t>;
    auto _web_load_file(string_view path) -> shared_ptr<string>;
};
}  // namespace perfkit::web::impl
