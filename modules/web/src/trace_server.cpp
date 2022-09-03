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
#include <cpph/refl/archive/json-reader.hxx>
#include <cpph/refl/archive/json-writer.hxx>
#include <cpph/refl/object.hxx>
#include <cpph/streambuf/string.hxx>
#include <cpph/streambuf/view.hxx>
#include <cpph/thread/event_queue.hxx>
#include <perfkit/traces.h>
#include <spdlog/spdlog.h>

namespace perfkit::backend {
spdlog::logger* nglog();
}

namespace perfkit::web::detail {
using namespace archive::decorators;

static auto CPPH_LOGGER() { return perfkit::backend::nglog(); }

class trace_server_impl : public trace_server
{
    using S = trace_server_impl;

    struct tracer_context {
        string cached_name_;  // Name can't be retrieved after tracer_ destroy.
        weak_ptr<tracer> tracer_;
        basic_event_queue<tracer::trace_fetch_proxy> receiver_{1 << 10};
    };

   private:
    class session : public if_websocket_session
    {
        trace_server_impl& s_;

       public:
        explicit session(trace_server_impl& s) : s_(s) {}

        void on_open() noexcept override
        {
            s_.ioc_.post(bind_weak(weak_from_this(), [this] {
                // Register this client to client list.
                s_.clients_.push_back(weak_from_this());

                // Send existing tracer list to client.
                auto wr = s_.ioc_writer_prepare_("tracer_instance_new");
                *wr << push_array(s_.all_.size());
                for (auto& ctx : s_.all_) { *wr << ctx->cached_name_; }
                *wr << pop_array;

                auto msg = s_.ioc_writer_done_();
                this->send_text(*msg);
            }));
        }

        void on_message(const string& message, bool is_binary) noexcept override
        {
            auto bin = s_.ioc_.allocate_temporary_payload(message.size());
            copy(message, bin.get());

            s_.ioc_.post(bind_weak(weak_from_this(), [this, bin = move(bin), len = message.size()] {
                s_.ioc_sess_recv_msg_(this, {bin.get(), len});
            }));
        }
    };

   private:
    event_queue& ioc_;
    vector<websocket_weak_ptr> clients_;
    vector<shared_ptr<tracer_context>> all_;
    map<string, weak_ptr<tracer_context>> name_table_;
    shared_ptr<void> anchor_ = make_shared<nullptr_t>();
    pool<tracer::fetched_traces> pool_nodes_;

    streambuf::stringbuf sbuf_;
    archive::json::writer json_wr_{&sbuf_};
    archive::json::reader json_rd_{nullptr};

   private:
    template <class Pred>
    void client_for_each_(Pred&& fn)
    {
        for (auto iter = clients_.begin(); iter != clients_.end();) {
            if (auto p_cli = iter->lock()) {
                fn(p_cli.get()), ++iter;
            } else {
                iter = clients_.erase(iter);
            }
        }
    }

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
    auto ioc_find_(weak_ptr<tracer> const& wp) const
    {
        return find_if(all_, [&wp](auto&& p) { return ptr_equals(p->tracer_, wp); });
    }

    void ioc_register_tracer_(tracer* ref)
    {
        if (all_.end() == ioc_find_(ref->weak_from_this())) {
            return;  // Already registered.
        }

        auto self = make_shared<tracer_context>();
        self->tracer_ = ref->weak_from_this();

        // Register given tracer instance.
        all_.push_back(self);
        name_table_.try_emplace(ref->name(), self);
        self->cached_name_ = ref->name();

        ref->on_destroy() << anchor_ << [this](tracer* tr) {
            ioc_.post([this, wp = tr->weak_from_this()] {
                auto iter = ioc_find_(wp);
                if (iter == all_.end()) { return; }

                auto name = move((**iter).cached_name_);
                name_table_.erase(name);
                all_.erase(iter);

                auto wr = ioc_writer_prepare_("tracer_instance_destroy");
                *wr << name;
                auto msg = ioc_writer_done_();

                // Send instance destroy message
                client_for_each_([&](auto* sess) {
                    sess->send_text(*msg);
                });
            });
        };

        ref->on_fetch() << self <<
                [this, self = self.get()]  //
                (tracer::trace_fetch_proxy proxy) {
                    self->receiver_.flush(proxy);
                };

        // Send new tracer registration message
        auto wr = ioc_writer_prepare_("tracer_instance_new");
        *wr << push_array(1) << ref->name() << pop_array;
        auto msg = ioc_writer_done_();

        client_for_each_([&](auto s) { s->send_text(*msg); });
    }

    archive::if_writer* ioc_writer_prepare_(string_view method)
    {
        sbuf_.clear(), json_wr_.clear();
        json_wr_ << push_object(2);
        json_wr_ << archive::key_next << "method" << method;
        json_wr_ << archive::key_next << "params";

        return &json_wr_;
    }

    std::string const* ioc_writer_done_()
    {
        json_wr_ << pop_object;
        json_wr_.flush();

        return &sbuf_.str();
    }

    static void goto_key(archive::json::reader* rd, string_view key)
    {
        if (not rd->goto_key(key))
            throw std::runtime_error("Key ["s.append(key) + "] missing!");
    }

    void ioc_handle_message_(session* sess, string_view method, archive::json::reader* rd)
    {
        if (method == "node_fetch") {
            goto_key(rd, "tracer");
            auto tracer_name = rd->read_as<string>();
            goto_key(rd, "fence_update");
            auto fence_update = rd->read_as<int64_t>();
            goto_key(rd, "fence_node_id");
            auto fence_node_id = rd->read_as<int64_t>();

            if (auto p_pair = find_ptr(name_table_, tracer_name)) {
                assert(not p_pair->second.expired());
                auto ctx = p_pair->second.lock();

                if (auto ref = ctx->tracer_.lock()) {
                    // Register receive on next update
                    ctx->receiver_.post(bind_front_weak(
                            sess->weak_from_this(),
                            [this, sess, fence_update, fence_node_id](tracer::trace_fetch_proxy fetcher) {
                                auto node_array = pool_nodes_.checkout();
                                fetcher.fetch_tree_diff(node_array.get(), fence_update);

                                ioc_.post(bind_front_weak(
                                        sess->weak_from_this(),
                                        [this, node_array = move(node_array), fence_node_id, sess] {
                                            ioc_handle_tracer_reply_(sess, *node_array, fence_node_id);
                                        }));
                            }));

                    // Trigger next fetch
                    ref->request_fetch_data();
                }
            }
        } else if (method == "node_control") {
            // TODO: Find tracer and deliver control message
        }
    }

    void ioc_handle_tracer_reply_(session* sess, tracer::fetched_traces const& traces, size_t fence_node_id)
    {
        // TODO: Calculate new nodes and publish to session

        // TODO: Publish updated nodes to session
    }

    void ioc_sess_recv_msg_(session* sess, string_view s)
    {
        // Parse incoming message
        streambuf::const_view sbuf{{s.data(), s.size()}};
        json_rd_.clear(), json_rd_.rdbuf(&sbuf);

        // RPC handling (method/params)
        try {
            auto key = json_rd_.begin_object();

            goto_key(&json_rd_, "method");
            auto method_name = json_rd_.read_as<string>();

            goto_key(&json_rd_, "params");
            ioc_handle_message_(sess, method_name, &json_rd_);

            json_rd_.end_object(key);
        } catch (std::exception& e) {
            CPPH_ERROR("! MESSAGE HANDLING ERROR: {}", e.what());
        }
    }
};

auto trace_server::create(if_web_terminal* p_term) noexcept -> ptr<trace_server>
{
    auto p_srv = make_unique<trace_server_impl>(p_term);
    p_srv->I_initialize_();
    return p_srv;
}
}  // namespace perfkit::web::detail
