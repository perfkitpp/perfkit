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
// Created by ki608 on 2022-03-19.
//

#include "config_context.hpp"

#include <spdlog/spdlog.h>

#include "asio/io_context.hpp"
#include "asio/post.hpp"
#include "cpph/refl/object.hxx"
#include "cpph/refl/rpc/rpc.hxx"
#include "perfkit/configs.h"
#include "perfkit/logging.h"

using namespace perfkit::net;
using std::forward;
using std::move;

namespace perfkit::net::detail {
spdlog::logger* nglog();
}

static auto CPPH_LOGGER()
{
    return detail::nglog();
}

void config_context::_publish_new_registry(shared_ptr<config_registry> rg)
{
    CPPH_DEBUG("Publishing registry '{}'", rg->name());

    auto& all = rg->bk_all();
    auto key = rg->name();
    auto root = message::notify::config_category_t{};
    root.name = key;

    // Always try destroy registries before
    _handle_registry_destruction(rg->name());

    auto registry_context = &_config_registries[rg->name()];
    registry_context->associated_keys.reserve(rg->bk_all().size());

    for (auto& [_, config] : all) {
        if (config->is_hidden())
            continue;  // dont' publish hidden ones

        auto hierarchy = config->tokenized_display_key();
        auto* level = &root;

        for (auto category : make_iterable(hierarchy.begin(), hierarchy.end() - 1)) {
            auto it = perfkit::find_if(level->subcategories,
                                       [&](auto&& s) { return s.name == category; });

            if (it == level->subcategories.end()) {
                level->subcategories.emplace_back();
                it = --level->subcategories.end();
                it->name = category;
            }

            level = &*it;
        }

        auto fn_archive =
                [](string* v, nlohmann::json const& js) {
                    v->clear();

                    if (js.empty())
                        return;

                    nlohmann::json::to_msgpack(js, nlohmann::detail::output_adapter<char>(*v));
                };

        auto* dst = &level->entities.emplace_back();
        auto config_key = config_key_t::hash(&*config);
        dst->name = config->tokenized_display_key().back();
        dst->description = config->description();
        dst->config_key = config_key.value;

        fn_archive(&dst->initial_value, config->serialize());
        fn_archive(&dst->opt_one_of, config->attribute().one_of);
        fn_archive(&dst->opt_max, config->attribute().max);
        fn_archive(&dst->opt_min, config->attribute().min);

        _config_instances[config_key] = config;
        registry_context->associated_keys.insert(config_key);
    }

    message::notify::new_config_category(_rpc).notify(rg->id(), key, root, _host->fn_basic_access());

    rg->on_update.add_weak(
            _monitor_anchor,
            [this](config_registry* rg, array_view<perfkit::detail::config_base*> updates) {
                auto wptr = rg->weak_from_this();

                // apply updates .. as long as wptr alive, pointers can be accessed safely
                auto buffer = std::vector(updates.begin(), updates.end());
                auto function = bind_front_weak(wptr, &self_t::_handle_update, this, rg, std::move(buffer));
                asio::post(*_event_proc, std::move(function));
            });

    rg->on_destroy.add_weak(
            _monitor_anchor,
            [this](config_registry* rg) {
                message::notify::deleted_config_category(_rpc).notify(rg->name());

                namespace views = ranges::views;
                std::vector<int64_t> keys_all;

                // retrive all keys from rg, and delete them from confmap
                auto function = perfkit::bind_front(
                        &self_t::_handle_registry_destruction, this, rg->name());
                asio::post(*_event_proc, std::move(function));
            });
}

void config_context::rpc_republish_all_registries()
{
    auto func =
            [this] {
                auto all_regs = config_registry::bk_enumerate_registries(true);
                for (auto& rg : all_regs)
                    _publish_new_registry(rg);
            };

    asio::post(*_event_proc, bind_front_weak(_monitor_anchor, std::move(func)));
}

void config_context::rpc_update_request(const message::config_entity_update_t& content)
{
    auto func =
            [this, content = content] {
                try {
                    auto& str = content.content_next;
                    auto json_content = nlohmann::json::from_msgpack(str);

                    auto config_key = config_key_t{content.config_key};
                    auto wptr = _config_instances.at(config_key);

                    if (auto cfg = wptr.lock()) {
                        cfg->request_modify(std::move(json_content));
                    } else {
                        CPPH_WARN("Trying to update expired config");
                    }
                } catch (nlohmann::json::parse_error& e) {
                    CPPH_INFO("Msgpack parsing failed: {}", e.what());
                } catch (std::out_of_range& e) {
                    CPPH_INFO("Config key {} out of range", content.config_key);
                }
            };

    asio::post(*_event_proc, bind_front_weak(_monitor_anchor, std::move(func)));
}

void config_context::start_monitoring(weak_ptr<void> anchor)
{
    _monitor_anchor = move(anchor);

    auto fn_handle_new_registry
            = [this](weak_ptr<config_registry> wptr) {
                  auto rg = wptr.lock();
                  if (not rg) { return; }

                  _publish_new_registry(rg);
              };

    auto fn_post_func
            = [this, fn_handle_new_registry](config_registry* rg) {
                  asio::post(*_event_proc, bind_front_weak(_monitor_anchor, fn_handle_new_registry, rg->weak_from_this()));
              };

    configs::on_new_config_registry()
            .add_weak(_monitor_anchor, std::move(fn_post_func));
}

void config_context::stop_monitoring()
{
    _monitor_anchor.reset();
}

void config_context::_handle_registry_destruction(const string& name)
{
    auto iter = _config_registries.find(name);
    if (iter == _config_registries.end()) { return; }

    CPPH_DEBUG("Registry '{}' is being destructed", name);

    for (auto& key : iter->second.associated_keys) {
        _config_instances.erase(key);
    }

    _config_registries.erase(iter);
}

void config_context::_handle_update(
        config_registry* rg,
        vector<perfkit::detail::config_base*> const& changes)
{
    bool const is_waiting = not _pending_updates.empty();

    for (auto ptr : changes) {
        auto weak = ptr->weak_from_this();
        auto iter = find_if(_pending_updates, [&](auto&& e) { return ptr_equals(e, weak); });
        if (iter == _pending_updates.end()) { _pending_updates.push_back(std::move(weak)); }
    }

    using std::chrono::steady_clock;
    using namespace std::literals;
    if (is_waiting) { return; }

    _lazy_update_publish.expires_after(10ms);
    _lazy_update_publish.async_wait(
            [this](auto&& ec) {
                if (ec) { return; }
                auto& payload = _buf_payload;
                for (auto& weak : _pending_updates) {
                    auto cfg = weak.lock();
                    if (not cfg) { continue; }

                    payload.config_key = config_key_t::hash(&*cfg).value;
                    payload.content_next.clear();
                    cfg->serialize([buf = &payload.content_next](nlohmann::json const& content) {
                        nlohmann::json::to_msgpack(content, nlohmann::detail::output_adapter<char>(*buf));
                    });

                    message::notify::config_entity_update(_rpc).notify(payload, _host->fn_basic_access());
                }

                _pending_updates.clear();
            });
}
