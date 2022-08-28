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

#include <cpph/std/set>
#include <cpph/std/unordered_map>

#include <cpph/container/alloca_fwd_list.hxx>
#include <cpph/refl/archive/json-reader.hxx>
#include <cpph/refl/archive/json-writer.hxx>
#include <cpph/refl/object.hxx>
#include <cpph/streambuf/string.hxx>
#include <cpph/thread/event_queue.hxx>
#include <cpph/utility/functional.hxx>
#include <cpph/utility/timer.hxx>

#include "perfkit/detail/configs-v2-backend.hpp"
#include "perfkit/logging.h"

using namespace cpph;
using namespace cpph::archive::decorators;

namespace perfkit::backend {
spdlog::logger* nglog();
}

static auto CPPH_LOGGER() { return perfkit::backend::nglog(); }

namespace perfkit::web::detail {
class config_server_impl : public config_server
{
    using self_t = config_server_impl;

    if_web_terminal* term_;
    event_queue& ioc_ = term_->event_queue();

    shared_ptr<void> anchor_ = make_shared<nullptr_t>();
    vector<websocket_weak_ptr> clients_;
    map<weak_ptr<config_registry>, string, std::owner_less<>> regs_;

    streambuf::stringbuf sbuf_;
    archive::json::writer json_wr_{&sbuf_};

   private:
    class config_session_impl : public if_websocket_session
    {
        config_server_impl& s_;

       public:
        explicit config_session_impl(config_server_impl* s) : s_(*s) {}

        void on_open() noexcept override
        {
            s_.ioc_.post([this, wself = weak_from_this()] {
                auto self = wself.lock();
                if (not self) { return; }

                // 1. Append to client list
                erase_if(s_.clients_, [](auto&& e) { return e.expired(); });
                s_.clients_.push_back(wself);

                // 2. Collect & garbage collect registries
                CPPH_ALLOCA_LIST(config_registry_ptr, regs);
                for (auto iter = s_.regs_.begin(); iter != s_.regs_.end();) {
                    if (auto p = iter->first.lock())
                        CPPH_ALLOCA_LIST_EMPLACE(regs, move(p)), ++iter;
                    else
                        iter = s_.regs_.erase(iter);
                }

                // 3. Send list of new registries
                {
                    auto& wr = *s_.ioc_writer_prepare_("new-root");
                    wr.array_push(regs.size());
                    for (auto& rg : regs) { wr << rg->name(); }
                    wr.array_pop();
                    this->send_text(*s_.ioc_writer_done_());
                }

                // 4. Send all config instances
                for (auto& rg : regs) {
                    this->send_text(*s_.ioc_generate_descriptor_all_(rg.get()));
                }
            });
        }

        void on_message(const string& message, bool is_binary) noexcept override
        {
            // TODO: Handle commit.
        }
    };

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
    explicit config_server_impl(if_web_terminal* term) : term_(term)
    {
        // Iterate all existing config regs, register observer.
        config_registry::backend_t::g_evt_registered()
                << anchor_
                << [this](auto pp) { ioc_.post(bind(&self_t::ioc_register_observer_, this, move(pp))); };

        config_registry::backend_t::g_evt_unregistered()
                << anchor_
                << [this](auto pp) { ioc_.post(bind(&self_t::ioc_unregister_observer_, this, move(pp))); };

        // Reserve initialization
        ioc_.post(
                [this] {
                    // Register all initial registries
                    vector<config_registry_ptr> all_reg;
                    config_registry::backend_t::bk_enumerate_registries(&all_reg);

                    for (auto& rg : all_reg) { ioc_register_observer_(rg); }
                });
    }

    auto new_session_context() -> websocket_ptr override
    {
        return make_shared<config_session_impl>(this);
    }

   private:
    void ioc_register_observer_(config_registry_ptr const& rg)
    {
        // Check duplication
        if (not regs_.try_emplace(rg, rg->name()).second) {
            return;  // Already registered!
        }

        // Register events
        rg->backend()->evt_structure_changed()
                << anchor_
                << [this, wp = weak_ptr{rg}](auto) {
                       ioc_.post([this, wp] {
                           erase_if(clients_, [](auto&& e) { return e.expired(); });
                           if (clients_.empty())
                               return;
                           if (auto rg = wp.lock()) {
                               // Re-generate descriptor of given registry class.
                               auto p_payload = ioc_generate_descriptor_all_(rg.get());
                               client_for_each_([&](auto cli) { cli->send_text(*p_payload); });
                           }
                       });
                   };

        rg->backend()->evt_updated_entities()
                << anchor_
                << [this, wp = weak_ptr{rg}](config_registry* rg, list<config_base_ptr> updates) {
                       ioc_.post([this, wp, updates = move(updates), wrg = rg->weak_from_this()] {
                           auto rg = wrg.lock();
                           if (not rg || clients_.empty())
                               return;

                           auto backend = rg->backend();
                           auto wr = ioc_writer_prepare_("update");
                           *wr << push_object(updates.size());
                           for (auto& cfg : updates) {
                               *wr << key << cfg->id().value
                                   << push_object(2);

                               *wr << key << "updateFence" << cfg->fence();
                               *wr << key << "value";
                               backend->bk_access_value_shared(
                                       cfg.get(), [&](auto& data) {
                                           *wr << data.view();
                                       });

                               *wr << pop_object;
                           }
                           *wr << pop_object;

                           client_for_each_([&, s = ioc_writer_done_()](auto cli) {
                               cli->send_text(*s);
                           });
                       });
                   };

        // GC Client first.
        erase_if(clients_, [](auto&& e) { return e.expired(); });

        // Publish new root event
        if (not clients_.empty()) {
            auto wr = ioc_writer_prepare_("new-root");
            *wr << push_array(1) << rg->name() << pop_array;
            client_for_each_([&, s = ioc_writer_done_()](auto cli) { cli->send_text(*s); });
        }
    }

    void ioc_unregister_observer_(weak_ptr<config_registry> const& observ)
    {
        if (auto iter = regs_.find(observ); iter == regs_.end()) {
            return;
        } else {
            *ioc_writer_prepare_("delete-root") << iter->second;
            client_for_each_([&, s = ioc_writer_done_()](auto cli) { cli->send_text(*s); });

            regs_.erase(iter);
        }
    }

    auto ioc_generate_descriptor_all_(config_registry* rg) -> string const*
    {
        vector<config_base_ptr> items;
        rg->backend()->bk_all_items(&items);

        auto wr = ioc_writer_prepare_("new-cfg");

        (*wr) << push_array(items.size());
        for (auto& item : items) {
            ioc_generate_descriptor_(rg, item.get(), wr);
        }
        (*wr) << pop_array;

        return ioc_writer_done_();
    }

    void ioc_generate_descriptor_(config_registry* rg, config_base* cfg, archive::if_writer* wr)
    {
        string full_key = cfg->full_key(), display_key;
        vector<string_view> hierarchy;
        _configs::parse_full_key(full_key, &display_key, &hierarchy);

        // Generate 'ElemDesc'
        *wr << push_object(10)
            << key << "elemID" << cfg->id().value
            << key << "description" << cfg->description()
            << key << "name" << cfg->name()
            << key << "path" << hierarchy
            << key << "rootName" << rg->name()
            << key << "initValue" << cfg->default_value().view()
            << key << "minValue" << write_or(cfg->attribute()->min.view(), nullptr)
            << key << "maxValue" << write_or(cfg->attribute()->max.view(), nullptr)
            << key << "oneOf" << write_or(cfg->attribute()->one_of.view(), nullptr)
            << key << "editMode" << write_or(cfg->attribute()->one_of.view(), nullptr);
        *wr << pop_object;
    }

    archive::if_writer* ioc_writer_prepare_(string_view method)
    {
        sbuf_.clear(), json_wr_.clear();
        json_wr_ << push_object(2);
        json_wr_ << archive::key_next << "method" << method;
        json_wr_ << archive::key_next << "param";

        return &json_wr_;
    }

    std::string const* ioc_writer_done_()
    {
        json_wr_ << pop_object;
        json_wr_.flush();

        return &sbuf_.str();
    }

    static string_view editor_type_to_str_(edit_mode m) noexcept
    {
        switch (m) {
            case edit_mode::none: return "none"sv;
            case edit_mode::trigger: return "trigger"sv;
            case edit_mode::path: return "path"sv;
            case edit_mode::path_file: return "path_file"sv;
            case edit_mode::path_file_multi: return "path_file_multi"sv;
            case edit_mode::path_dir: return "path_dir"sv;
            case edit_mode::script: return "script"sv;
            case edit_mode::color_b: return "color_b"sv;
            case edit_mode::color_f: return "color_f"sv;
        }
    }
};

auto config_server::create(if_web_terminal* p) noexcept -> cpph::ptr<config_server>
{
    return make_unique<config_server_impl>(p);
}
}  // namespace perfkit::web::detail
