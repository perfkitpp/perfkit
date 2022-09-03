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
// Created by ki608 on 2022-09-03.
//

#include "trace_server.hpp"

#include <cpph/memory/pool.hxx>
#include <cpph/refl/object.hxx>
#include <cpph/thread/event_queue.hxx>
#include <perfkit/traces.h>

namespace perfkit::web::detail {
class trace_server_impl : public trace_server
{
    using S = trace_server_impl;

    struct tracer_context {
        weak_ptr<tracer> tracer_;
        basic_event_queue<tracer::trace_fetch_proxy> receiver_;
    };

   private:
    class session : public if_websocket_session
    {
        trace_server_impl& s_;

       public:
        explicit session(trace_server_impl& s) : s_(s) {}

        void on_open() noexcept override
        {
            // TODO: From all active tracers ...
            //  - Iterate all active tracers
            //  - Publish all tracer contents
            //  - Iterate for each tracers, register 'cache' event.
        }

        void on_message(const string& message, bool is_binary) noexcept override
        {
            // TODO: 'fetch-next'
            // TODO: 'node-flag'
        }
    };

   private:
    event_queue& ioc_;
    vector<shared_ptr<tracer_context>> all_;
    map<string, weak_ptr<tracer_context>> name_table_;
    shared_ptr<void> anchor_ = make_shared<nullptr_t>();

   public:
    explicit trace_server_impl(if_web_terminal* p_term)
            : ioc_(p_term->event_queue())
    {
    }

    void I_initialize_()
    {
        tracer::on_new_tracer()() << anchor_ << [this](tracer* tr) {
            ioc_.post([this, wp = tr->weak_from_this()] {
                auto tr = wp.lock();
                if (not tr) { return; }

                ioc_register_tracer_(tr.get());
            });
        };

        auto all_now = tracer::all();
        for (auto& tr : all_now) {
            ioc_register_tracer_(tr.get());
        }
    }

    auto new_session_context() -> websocket_ptr override
    {
        return make_shared<session>(*this);
    }

   private:
    static auto make_tracer_pred(weak_ptr<tracer> const& wp)
    {
        return [&wp](auto&& p) { return ptr_equals(p->tracer_, wp); };
    }

    void ioc_register_tracer_(tracer* ref)
    {
        if (ioc_find_(ref->weak_from_this())) {
            return;  // Already registered.
        }

        ref->on_destroy() << anchor_ << [this](tracer* tr) {
            ioc_.post([this, wp = tr->weak_from_this()] {
                auto iter = find_if(all_, make_tracer_pred(wp));
                if (iter == all_.end()) { return; }

                all_.erase(iter);
            });
        };

        ref->on_fetch() << anchor_ << [this](tracer::trace_fetch_proxy proxy) {

        };
    }

    auto ioc_find_(weak_ptr<tracer> const& wp) const -> shared_ptr<tracer_context>
    {
        auto iter = find_if(all_, make_tracer_pred(wp));
        if (iter == all_.end()) { return nullptr; }

        return *iter;
    }
};

auto trace_server::create(if_web_terminal* p_term) noexcept -> ptr<trace_server>
{
    auto p_srv = make_unique<trace_server_impl>(p_term);
    p_srv->I_initialize_();
    return p_srv;
}
}  // namespace perfkit::web::detail
