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

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include "cpph/helper/quick_bench.hxx"
#include "cpph/memory/list_pool.hxx"
#include "cpph/refl/archive/msgpack-reader.hxx"
#include "cpph/refl/archive/msgpack-writer.hxx"
#include "cpph/refl/object.hxx"
#include "cpph/refl/rpc/rpc.hxx"
#include "cpph/utility/hasher.hxx"
#include "cpph/utility/timer.hxx"
#include "perfkit/detail/configs-v2-backend.hpp"
#include "perfkit/logging.h"

using namespace perfkit::net;
using std::forward;
using std::move;

namespace perfkit::backend {
spdlog::logger* nglog();
}

static auto CPPH_LOGGER()
{
    return perfkit::backend::nglog();
}

void config_context::start_monitoring(weak_ptr<void> anchor)
{
    _monitor_anchor = move(anchor);

    // Register 'new repo' event
    config_registry::backend_t::g_evt_registered.add_weak(
            _monitor_anchor, [this](config_registry_ptr ptr) {
                if (assert(ptr), not ptr) { return; }

                post(*_ioc, [this, ptr = move(ptr)] {
                    auto [iter, is_new] = _nodes.try_emplace(weak_ptr{ptr});

                    if (is_new) {
                        // Guarantees single invocation
                        _init_registry_node(iter, ptr.get());
                    }

                    _update_registry_structure(ptr.get(), iter);
                    _publish_registry_refresh(iter);
                });
            });

    config_registry::backend_t::g_evt_unregistered.add_weak(
            _monitor_anchor, [this](config_registry_wptr wp) {
                post(*_ioc, [this, wp = move(wp)] {
                    auto iter = _nodes.find(wp);
                    if (iter == _nodes.end()) { return; }

                    _publish_unregister(iter);
                    _nodes.erase(iter);
                    _gc_mappings();
                });
            });

    // Register all existing repositories
    vector<v2::config_registry_ptr> registries;
    config_registry::backend_t::bk_enumerate_registries(&registries, false);

    for (auto& rg : registries) {
        if (assert(rg), not rg) { continue; }

        auto [iter, is_new] = _nodes.try_emplace(rg);

        if (is_new) {
            _init_registry_node(iter, rg.get());
        }

        _update_registry_structure(rg.get(), iter);
    }
}

void config_context::stop_monitoring()
{
    _monitor_anchor.reset();

    // Reset all cached contexts
    _nodes.clear();
}

void config_context::rpc_republish_all_registries()
{
    post(*_ioc, [this] {
        // Republish all contexts
        for (auto iter = _nodes.begin(), end = _nodes.end(); iter != end; ++iter)
            _publish_registry_refresh(iter);
    });
}

void config_context::rpc_update_request(message::config_entity_update_t& content)
{
    post(*_ioc, [this, content] {
        auto elem = find_ptr(_inv_mapping, content.config_key);
        if (not elem) { return; }

        auto cfg = elem->second.lock();
        if (not cfg) { return; }

        auto owner = cfg->owner();
        if (not owner) { return; }

        streambuf::view sbuf{{(char*)content.content_next.data(), content.content_next.size()}};
        archive::msgpack::reader reader{&sbuf};
        owner->backend()->bk_commit(cfg.get(), &reader);
    });
}

void config_context::_init_registry_node(
        registry_table_type::iterator node, config_registry* ptr)
{
    // Update all structure
    ptr->backend()->evt_structure_changed.add_weak(
            _monitor_anchor, [this](config_registry* rg) {
                post(*_ioc, [this, wrg = rg->weak_from_this()] {
                    auto rg = wrg.lock();
                    if (rg == nullptr) { return; }

                    auto iter = _nodes.find(wrg);
                    if (iter == _nodes.end()) { return; }

                    _update_registry_structure(rg.get(), iter);
                    _publish_registry_refresh(iter);
                });
            });

    ptr->backend()->evt_updated_entities.add_weak(
            _monitor_anchor, [this](config_registry* rg, list<config_base_ptr> const& l) {
                vector<config_base_ptr> updates;
                updates.reserve(l.size());
                updates.assign(l.begin(), l.end());

                post(*_ioc, bind_front_weak(_monitor_anchor,
                                            &config_context::_publish_updates,
                                            this,
                                            rg->shared_from_this(),
                                            move(updates)));
            });
}

void config_context::_update_registry_structure(
        config_registry* rg,
        const registry_table_type::iterator& table_iterator)
{
    stopwatch swatch;

    vector<config_base_ptr> configs;
    rg->backend()->bk_all_items(&configs);

    list_pool<message::notify::config_category_t> reuse_subc;
    list_pool<message::config_entity_t> reuse_entity;

    auto* p_root = &table_iterator->second.cat_root;
    p_root->name = rg->name();
    p_root->category_id = hasher::fnv1a_64(rg->id().value);

    reuse_subc.checkin(&p_root->subcategories);
    reuse_entity.checkin(&p_root->entities);

    /// Iterate all configs, then
    // 1. Create category name hierarchy from full key
    // 2. Search hierarchy insertion position from category tree.
    // 3. Insert entity on appropriate position
    // note: Sort order will be automatically applied by above routine.

    // Caches
    string full_key;
    string display_key;
    vector<string_view> hierarchy_;
    streambuf::stringbuf msbuf;
    archive::msgpack::writer mwrite{&msbuf};

    auto serialize =
            [&](string* dst, refl::object_const_view_t view) {
                dst->clear();
                msbuf.reset(dst);
                mwrite << view;
                mwrite.flush();
            };

    // Optimize lookup overhead using lut
    _tmp_lut.clear();

    for (auto& cfg : configs) {
        if (cfg->attribute()->hidden)
            continue;

        cfg->full_key(&full_key);
        _inv_mapping[cfg->id().value] = cfg;

        // (1)
        _configs::parse_full_key(full_key, &display_key, &hierarchy_);

        array_view hierarchy = hierarchy_;
        hierarchy = hierarchy.subspan(0, hierarchy.size() - 1);
        auto category_id = hasher::fnv1a_64(hierarchy, p_root->category_id);
        auto parent_node = p_root;

        // (2)
        if (hierarchy.empty()) {
            ;
        } else if (auto vp_category = find_ptr(_tmp_lut, category_id)) {
            parent_node = static_cast<decltype(parent_node)>(vp_category->second);
        } else {
            for (; not hierarchy.empty();) {
                auto subc_name = hierarchy.front();
                auto psubc = &parent_node->subcategories;

                // Try find subcategory name from current parent node's subcategories
                auto iter = find_if(*psubc, [&](auto&& n) { return n.name == subc_name; });

                if (iter == psubc->end()) {
                    // If category did not exist, init it.
                    iter = reuse_subc.checkout(psubc, psubc->end());
                    iter->name = subc_name;
                    iter->category_id = hasher::fnv1a_64(subc_name, parent_node->category_id);
                    reuse_subc.checkin(&iter->subcategories);
                    reuse_entity.checkin(&iter->entities);

                    auto is_new = _tmp_lut.try_emplace(iter->category_id, &*iter).second;
                    assert(is_new && "LUT must be unique!"), (void)is_new;
                }

                parent_node = &*iter;
                hierarchy = hierarchy.subspan(1);  // Consume front-most hierarchy name
            }
        }

        // (3)
        auto* entity = &reuse_entity.checkout_back(&parent_node->entities);
        entity->name = cfg->name();
        entity->description = const_buffer_view{cfg->description()};
        entity->config_key = cfg->id().value;

        // Serialize attributes
        auto& attrib = cfg->attribute();
        entity->edit_mode = attrib->edit_mode;
        serialize(&entity->initial_value, attrib->default_value.view());

        if (attrib->min)
            serialize(&entity->opt_min, attrib->min.view());
        if (attrib->max)
            serialize(&entity->opt_max, attrib->max.view());
        if (attrib->one_of)
            serialize(&entity->opt_one_of, attrib->one_of.view());
    }

    CPPH_DEBUG("Updated registry {} ... {:.2f} ms for categorizing {} elements",
               rg->name(),
               swatch.elapsed().count() * 1e3,
               configs.size());
}

void config_context::_publish_registry_refresh(registry_table_type::iterator const& iter)
{
    auto rg = iter->first.lock();
    assert(rg && "This method must be called when registry is certainly valid!");
    message::notify::update_config_category(_rpc).notify(
            rg->id().value, rg->name(), iter->second.cat_root, _host->fn_basic_access());

    vector<config_base_ptr> configs;
    rg->backend()->bk_all_items(&configs);
    _publish_updates(rg, configs);
}

void config_context::_publish_updates(shared_ptr<config_registry> rg, vector<shared_ptr<config_base>> const& updates)
{
    message::config_entity_update_t update_content;
    streambuf::stringbuf sbuf{&update_content.content_next};
    archive::msgpack::writer writer{&sbuf};

    auto bk = rg->backend();
    for (auto& cfg : updates) {
        update_content.config_key = cfg->id().value;

        sbuf.clear(), writer.clear();
        bk->bk_access_value_shared(cfg.get(), [&](auto&& raw) {
            writer << raw.view();
        });
        writer.flush();

        message::notify::config_entity_update(_rpc).notify(
                update_content, _host->fn_basic_access());
    }
}

void config_context::_publish_unregister(registry_table_type::iterator node)
{
    message::notify::deleted_config_category(_rpc).notify(node->second.cat_root.name);
}

void config_context::_gc_mappings()
{
    erase_if_each(_inv_mapping, [](auto&& pair) { return pair.second.expired(); });
}
