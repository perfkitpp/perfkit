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
// Created by ki608 on 2022-08-28.
//

#include "config_server.hpp"

#include <cpph/thread/event_queue.hxx>

#include "perfkit/detail/configs-v2-backend.hpp"
#include "perfkit/logging.h"

namespace perfkit::backend {
spdlog::logger* nglog();
}

static auto CPPH_LOGGER() { return perfkit::backend::nglog(); }

namespace perfkit::web::detail {

class config_server_impl : public config_server
{
    if_web_terminal* term_;
    event_queue& ioc_ = term_->event_queue();

    shared_ptr<void> anchor_;
    vector<websocket_weak_ptr> clients_;
    map<string, config_registry_ptr, std::less<>> regs_;

   private:
    class config_session_impl : public if_websocket_session
    {
        config_server_impl& s_;

       public:
        explicit config_session_impl(config_server_impl* s) : s_(*s) {}

        void on_open() noexcept override
        {
            s_.ioc_.post([this] {
                // TODO: Flush all available configs
            });
        }

        void on_message(const string& message, bool is_binary) noexcept override
        {
            if_websocket_session::on_message(message, is_binary);
        }

        void on_close(const string& why) noexcept override
        {
            if_websocket_session::on_close(why);
        }

        void on_error(const string& what) noexcept override
        {
            if_websocket_session::on_error(what);
        }
    };

   public:
    explicit config_server_impl(if_web_terminal* term) : term_(term)
    {
    }

    auto new_session_context() -> websocket_ptr override
    {
        return make_shared<config_session_impl>(this);
    }

    void commit(string const& path, string const& payload) override
    {
    }

   private:
};

auto config_server::create(if_web_terminal* p) noexcept -> cpph::ptr<config_server>
{
    return make_unique<config_server_impl>(p);
}
}  // namespace perfkit::web::detail
