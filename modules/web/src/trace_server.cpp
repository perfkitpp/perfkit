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

    struct tracer_context {
        string cached_name_;  // Name can't be retrieved after tracer_ destroy.
        weak_ptr<tracer> tracer_;

        tracer::fetched_traces traces_;
        vector<bool> dirty_;
        atomic<uint64_t> fence_update_ = 0;

        // Tuples are in order: weak ptr to session, fence_update, fence_node_id
        vector<tuple<weak_ptr<session>, uint64_t, uint64_t>> waiting_sessions_;
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
                fn(p_cli), ++iter;
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
        if (all_.end() != ioc_find_(ref->weak_from_this())) {
            return;  // Already registered.
        }

        auto self = make_shared<tracer_context>();
        self->tracer_ = ref->weak_from_this();

        // Register given tracer instance.
        all_.push_back(self);
        name_table_[ref->name()] = self;
        self->cached_name_ = ref->name();

        ref->on_destroy() << anchor_ << [this](tracer* tr) {
            ioc_.post(bind(&S::ioc_handle_unregister_, this, tr->weak_from_this()));
        };

        ref->on_fetch() << self <<
                [this, w_self = weak_ptr{self}]  //
                (tracer::trace_fetch_proxy proxy) {
                    auto self = w_self.lock();
                    auto fence_update = relaxed(self->fence_update_);
                    auto buffer = pool_nodes_.checkout();

                    proxy.fetch_tree_diff(buffer.get(), fence_update);
                    ioc_.post([this, w_self, buffer = move(buffer)] {
                        auto self = w_self.lock();
                        if (not self) { return; }

                        ioc_on_fetch_(self.get(), *buffer);
                    });
                };

        // Send new tracer registration message
        auto wr = ioc_writer_prepare_("tracer_instance_new");
        *wr << push_array(1) << ref->name() << pop_array;
        auto msg = ioc_writer_done_();

        client_for_each_([&](auto s) { s->send_text(*msg); });
    }

    void ioc_handle_unregister_(weak_ptr<tracer> const& wp)
    {
        auto iter = ioc_find_(wp);
        if (iter == all_.end()) { return; }

        auto name = move((**iter).cached_name_);
        name_table_.erase(name);
        all_.erase(iter);

        auto wr = ioc_writer_prepare_("tracer_instance_destroy");
        *wr << name;
        auto msg = ioc_writer_done_();

        // Send instance destroy message
        client_for_each_([&](auto&& sess) {
            sess->send_text(*msg);
        });
    }

    void ioc_on_fetch_(tracer_context* tc, tracer::fetched_traces& diff)
    {
        auto* traces = &tc->traces_;

        // At least, traces array should be larger than received array.
        traces->reserve(diff.size());

        //
        uint64_t new_fence_update = 0;
        uint64_t new_fence_id = 0;

        // Update existing cache.
        for (auto& trace : diff) {
            new_fence_update = std::max(new_fence_update, trace.fence);
            new_fence_id = std::max(new_fence_id, trace.unique_order);

            if (trace.unique_order >= traces->size()) {
                traces->resize(trace.unique_order + 1);
            }

            (*traces)[trace.unique_order] = move(trace);
        }

        // Update update fence value.
        release(tc->fence_update_, new_fence_update + 1);
        tc->dirty_.resize(tc->traces_.size());  // Always fit to trace count

        // Iterate clients, publish fetched contents
        vector<size_t> idx_updated_node;  // Index of new nodes that were added.
        idx_updated_node.reserve(traces->size());

        for (auto& [wp, fence_update, fence_node_id] : tc->waiting_sessions_) {
            auto client = wp.lock();
            if (not client) { continue; }

            // Publish all newly added nodes
            if (fence_node_id < traces->size()) {
                auto wr = ioc_writer_prepare_("node_new");
                *wr << push_object(3);
                *wr << key << "tracer" << tc->cached_name_;
                *wr << key << "fence_node_id" << traces->size();
                *wr << key << "nodes" << push_array(traces->size() - fence_node_id);

                for (auto idx : count(fence_node_id, traces->size())) {
                    auto& trace = (*traces)[idx];

                    *wr << push_object(4);
                    {
                        assert(trace.unique_order == idx);
                        *wr << key << "unique_index" << trace.unique_order;
                        *wr << key << "parent_index" << (trace.owner_node ? trace.owner_node->unique_order : -1);
                        *wr << key << "name" << trace.key;
                        *wr << key << "path" << trace.hierarchy;
                    }
                    *wr << pop_object;
                }

                *wr << pop_array << pop_object;
                client->send_text(*ioc_writer_done_());
            }

            // Iterate all nodes, check if there's any available update.
            idx_updated_node.clear();

            for (auto& trace : *traces) {
                if (fence_update < trace.fence || tc->dirty_[trace.unique_order]) {
                    idx_updated_node.push_back(trace.unique_order);
                }
            }

            // Clear dirty flag array
            fill(tc->dirty_, false);

            // Publish updates
            if (not idx_updated_node.empty()) {
                auto wr = ioc_writer_prepare_("node_update");
                *wr << push_object(3);
                *wr << key << "tracer" << tc->cached_name_;
                *wr << key << "fence_update" << new_fence_update;
                *wr << key << "nodes" << push_array(idx_updated_node.size());

                for (auto idx : idx_updated_node) {
                    auto& trace = (*traces)[idx];

                    *wr << push_array(2) << idx;
                    *wr << push_object(4);
                    {
                        *wr << key << "f_F" << trace.folded();
                        *wr << key << "f_S" << trace.subscribing();
                        *wr << key << "T";

                        switch (trace.data.index()) {
                            case 0:  // nullptr_t
                                *wr << "P" << key << "V" << nullptr;
                                break;
                            case 1:  // duration
                                *wr << "T" << key << "V" << to_seconds(get<steady_clock::duration>(trace.data));
                                break;
                            case 2:  // integer
                                *wr << "P" << key << "V" << get<int64_t>(trace.data);
                                break;
                            case 3: {  // double
                                auto const val = get<double>(trace.data);
                                if (isnan(val) || isinf(val)) {
                                    *wr << "P" << key << "V" << (isnan(val) ? "NaN" : (val > 0 ? "+INF" : "-INF"));
                                } else {
                                    *wr << "P" << key << "V" << val;
                                }
                            } break;
                            case 4:  // string
                                *wr << "P" << key << "V" << get<string>(trace.data);
                                break;
                            case 5:  // boolean
                                *wr << "P" << key << "V" << get<bool>(trace.data);
                                break;
                            default:
                                CPPH_WARN("INVALID VARIANT INDEX!!!");
                                *wr << "P" << key << "V" << nullptr;
                                break;
                        }
                    }
                    *wr << pop_object;
                    *wr << pop_array;
                }

                *wr << pop_array << pop_object;
                client->send_text(*ioc_writer_done_());
            }
        }

        // Clear queue
        tc->waiting_sessions_.clear();
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
            rd->begin_object();

            goto_key(rd, "tracer");
            auto tracer_name = rd->read_as<string>();
            goto_key(rd, "fence_update");
            auto fence_update = rd->read_as<int64_t>();
            goto_key(rd, "fence_node_id");
            auto fence_node_id = rd->read_as<int64_t>();

            auto ctx = name_table_.at(tracer_name).lock();

            if (auto ref = ctx->tracer_.lock()) {
                // Register receive on next update
                // Registration must be unique, thus find if there's any other registration already.
                auto iter = find_if(ctx->waiting_sessions_, [wp = sess->weak_from_this()](auto&& p) { return ptr_equals(get<0>(p), wp); });
                if (iter == ctx->waiting_sessions_.end()) {
                    auto wp = weak_ptr{static_pointer_cast<session>(sess->shared_from_this())};
                    iter = ctx->waiting_sessions_.insert(ctx->waiting_sessions_.end(), make_tuple(wp, fence_update, fence_node_id));
                } else {
                    get<1>(*iter) = fence_update;
                    get<2>(*iter) = fence_node_id;
                }

                // Request next fetch. This function may be invoked multiple times.
                ref->request_fetch_data();
            } else {
                CPPH_WARN("! Bad dropped tracer detected ... re-validating all!");
                ioc_.post(bind(&S::ioc_handle_unregister_, this, ctx->tracer_));
                ioc_.post([this] { for(auto& p : tracer::all()) { ioc_register_tracer_(p.get());} });
            }
        } else if (method == "node_control") {
            // Find tracer and deliver control message
            rd->begin_object();

            goto_key(rd, "tracer");
            auto tracer_name = rd->read_as<string>();
            goto_key(rd, "node_index");
            auto node_index = rd->read_as<int64_t>();

            auto ctx = name_table_.at(tracer_name).lock();
            auto node = &ctx->traces_.at(node_index);

            if (rd->goto_key("fold"))
                node->fold(rd->read_as<bool>());

            if (rd->goto_key("subscribe"))
                node->subscribe(rd->read_as<bool>());

            // Mark element dirty, which will forcibly uploaded on next fetch.
            ctx->dirty_.at(node_index) = true;
        }
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
