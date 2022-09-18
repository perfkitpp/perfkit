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
// Created by ki608 on 2022-09-17.
//

#include "graphic_server.hpp"

#include <cpph/std/set>
#include <utility>

#include <cpph/memory/pool.hxx>
#include <cpph/refl/archive/json-reader.hxx>
#include <cpph/refl/archive/json-writer.hxx>
#include <cpph/refl/object.hxx>
#include <cpph/refl/types/binary.hxx>
#include <cpph/refl/types/tuple.hxx>
#include <cpph/streambuf/string.hxx>
#include <cpph/streambuf/view.hxx>
#include <cpph/third/stb.hpp>
#include <cpph/thread/event_queue.hxx>
#include <perfkit/detail/graphic-impl.hpp>
#include <spdlog/spdlog.h>

namespace perfkit::backend {
spdlog::logger* nglog();
}

namespace perfkit::web::detail {
using perfkit::detail::window_impl;

using namespace archive::decorators;
static auto CPPH_LOGGER() { return perfkit::backend::nglog(); }

class graphic_server_impl : public graphic_server
{
    using S = graphic_server_impl;

    class session : public if_websocket_session
    {
        graphic_server_impl* self_;
        archive::json::reader json_rd_{nullptr};

       public:
        explicit session(graphic_server_impl* self) : self_(self)
        {
        }
        void on_message(const string& message, bool is_binary) noexcept override
        {
            streambuf::view sbuf{view_array(const_cast<string&>(message))};
            json_rd_.clear(), json_rd_.rdbuf(&sbuf);

            // RPC handling (method/params)
            try {
                auto key = json_rd_.begin_object();

                json_rd_.jump("method");
                auto method_name = json_rd_.read_as<string>();

                json_rd_.jump("params");
                handle_msg_(method_name, &json_rd_);

                json_rd_.end_object(key);
            } catch (std::exception& e) {
                CPPH_ERROR("! MESSAGE HANDLING ERROR: {}", e.what());
            }
        }
        void on_open() noexcept override
        {
            self_->ioc_.post(bind_weak(weak_from_this(), &S::on_new_session_, self_, this));
        }
        void on_close(const string& why) noexcept override
        {
            self_->ioc_.post(bind_weak(weak_from_this(), &S::on_close_session_, self_, weak_from_this()));
        }

       private:
        void handle_msg_(string_view method, archive::json::reader* rd)
        {
            rd->begin_object();
            auto id = (rd->jump("id"), rd->read_as<size_t>());

            if (method == "wnd_input") {
                // TODO: Retrieve input events from stream, notify to host node ...
            } else if (method == "wnd_control") {
                // Retrieve watch state, if exist
                if (rd->goto_key("watching")) {
                    self_->ioc_.post(bind_weak(
                            weak_from_this(),
                            &S::set_watch_state_,
                            self_, weak_from_this(), id, rd->read_as<bool>()));
                }
            }
        }
    };

    struct window_node {
        string path_cached;

        weak_ptr<window_impl> w_wnd;
        set<websocket_weak_ptr, std::owner_less<>> watchers;

        const_image_buffer raw;
        binary<string> jpeg_cached;

        //
        bool is_jpeg_dirty = false;
    };

   private:
    if_web_terminal* term_;
    event_queue& ioc_ = term_->event_queue();

    vector<websocket_weak_ptr> clients_;
    map<size_t, window_node> wnds_;

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
    explicit graphic_server_impl(if_web_terminal* term) : term_(term)
    {
    }

    auto new_session_context() -> websocket_ptr override
    {
        return make_shared<session>(this);
    }

    void I_init_()
    {
        auto anchor = term_->anchor();

        // Register global register event
        window_impl::B_on_unregister()
                << anchor
                << bind_event_queue_weak(ioc_, anchor, &S::on_wnd_unregister_, this);

        window_impl::B_on_register()
                << anchor
                << bind_event_queue_weak(ioc_, anchor, &S::on_wnd_register_, this);

        window_impl::B_on_buffer_update()
                << anchor
                << bind_event_queue_weak(ioc_, anchor, &S::on_wnd_update_, this);

        // Iterate all existing windows, register event.
        vector<shared_ptr<window_impl>> all;
        window_impl::B_all_wnds(&all);

        for (auto& p_wnd : all) { on_wnd_register_(p_wnd.get()); }
    }

   private:
    void on_wnd_register_(window_impl* p_wnd)
    {
        auto [iter, is_new] = wnds_.try_emplace(p_wnd->id());
        if (not is_new) { return; }  // Already added!

        auto* node = &iter->second;
        node->path_cached = p_wnd->path();
        node->w_wnd = static_pointer_cast<window_impl>(p_wnd->shared_from_this());
        node->raw = p_wnd->latest_image();
        node->is_jpeg_dirty = not node->raw.empty();

        if (not clients_.empty()) {
            auto wr = ioc_writer_prepare_("new_windows");
            *wr << push_array(1) << make_pair(p_wnd->id(), string_view{p_wnd->path()}) << pop_array;
            client_for_each_([str = ioc_writer_done_()](auto&& cli) { cli->send_text(*str); });
        }
    }

    void on_wnd_unregister_(window_impl* p_wnd)
    {
        auto iter = wnds_.find(p_wnd->id());
        if (iter == wnds_.end()) { return; }  // Unregistered before register ...

        wnds_.erase(iter);  // Remove from nodes ...

        if (not clients_.empty()) {
            auto wr = ioc_writer_prepare_("deleted_window");
            *wr << p_wnd->id();
            client_for_each_([str = ioc_writer_done_()](auto&& cli) { cli->send_text(*str); });
        }
    }

    void on_wnd_update_(window_impl* p_wnd, const_image_buffer buff)
    {
        auto iter = wnds_.find(p_wnd->id());
        if (iter == wnds_.end()) {
            CPPH_WARN("* Window {}({}) is missing from management table ... possibly logic error!", p_wnd->id(), p_wnd->path());
            return;
        }

        auto* node = &iter->second;
        node->raw = std::move(buff);
        node->is_jpeg_dirty = not node->raw.empty();

        if (not node->is_jpeg_dirty) {
            // On empty image, do not propagate update.
            return;
        }

        // Iterate watching clients
        for (auto iter_watch = node->watchers.begin(); iter_watch != node->watchers.end();) {
            if (auto watcher = iter_watch->lock()) {
                try_publish_(p_wnd, node, watcher.get());
            } else {
                iter_watch = node->watchers.erase(iter_watch);
            }
        }
    }

    void try_publish_(window_impl* p_wnd, window_node* p_node, if_websocket_session* sess)
    {
        // Ensure single encode sequence is performed for multiple sessions
        if (exchange(p_node->is_jpeg_dirty, false)) {
            // 8 byte: ID, 56 byte: Reserved, rest: Jpeg payload
            p_node->jpeg_cached.clear();

            uint64_t id = p_wnd->id();
            p_node->jpeg_cached.resize(64);
            memcpy(p_node->jpeg_cached.data(), &id, sizeof id);

            auto& img = p_node->raw;
            stbi::write_jpg_to_func(
                    &S::jpeg_write_func_, p_node,
                    img.width, img.height, img.channels,
                    img.data.get(), p_wnd->B_get_quality());
        }

        // Send prepared payload to client
        sess->send_binary(p_node->jpeg_cached.ref());
    }

    static void jpeg_write_func_(void* context, void* data, int size)
    {
        auto p_buf = &((window_node*)context)->jpeg_cached;
        p_buf->insert(p_buf->end(), (uint8_t*)data, (uint8_t*)data + size);
    }

    void on_new_session_(session* p_sess)
    {
        // Iterate existing nodes, publish initially available window list.
        auto wr = ioc_writer_prepare_("new_windows");
        *wr << push_array(wnds_.size());
        for (auto& [id, node] : wnds_) {
            *wr << make_pair(id, string_view{node.path_cached});
        }
        *wr << pop_array;
        p_sess->send_text(*ioc_writer_done_());
    }

    void on_close_session_(websocket_weak_ptr const& wp)
    {
        // Iterate all nodes, unregister from all watches
        for (auto& [id, node] : wnds_) {
            set_watch_state_impl_(wp, &node, false);
        }
    }

    void set_watch_state_(websocket_weak_ptr const& wp, size_t id, bool state)
    {
        if (auto p_pair = find_ptr(wnds_, id)) {
            auto* p_node = &p_pair->second;
            set_watch_state_impl_(wp, p_node, state);
        }
    }

    void set_watch_state_impl_(websocket_weak_ptr const& wp, window_node* p_node, bool state)
    {
        auto p_wnd = p_node->w_wnd.lock();
        if (not p_wnd) {
            CPPH_WARN("! Destroying window '{}'", p_node->path_cached);
            return;
        }

        if (state) {
            auto was_empty = p_node->watchers.empty();
            if (p_node->watchers.emplace(wp).second && was_empty) {
                p_wnd->B_set_watching(true);
            }
        } else {
            if (p_node->watchers.erase(wp) && p_node->watchers.empty()) {
                p_wnd->B_set_watching(false);
            }
        }
    }

   private:
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
};

auto graphic_server::create(if_web_terminal* p_term) noexcept -> ptr<graphic_server>
{
    auto p = make_unique<graphic_server_impl>(p_term);
    p->I_init_();
    return p;
}
}  // namespace perfkit::web::detail
