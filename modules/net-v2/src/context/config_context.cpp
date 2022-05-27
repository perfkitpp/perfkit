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
#include <spdlog/spdlog.h>

#include "cpph/refl/object.hxx"
#include "cpph/refl/rpc/rpc.hxx"
#include "perfkit/detail/configs-v2-backend.hpp"
#include "perfkit/logging.h"

using namespace perfkit::net;
using std::forward;
using std::move;

namespace perfkit::net::detail {
spdlog::logger* nglog();
}

void config_context::start_monitoring(weak_ptr<void> anchor)
{
    _monitor_anchor = move(anchor);

    // Register 'new repo' event
    config_registry::backend_t::g_evt_registered.add_weak(
            _monitor_anchor, [this](config_registry_ptr ptr) {
                post(*_ioc, [this, ptr = move(ptr)] {
                    auto [iter, is_new] = _nodes.try_emplace(weak_ptr{ptr});

                    if (is_new) {
                        // Guarantees single invocation
                        _init_registry_node(iter, ptr.get());
                    }

                    _update_registry_structure(iter);
                    _publish_registry_refresh(iter);
                });
            });

    config_registry::backend_t::g_evt_unregistered.add_weak(
            _monitor_anchor, [this](config_registry_wptr wp) {
                post(*_ioc, [this, wp = move(wp)] {
                    auto iter = _nodes.find(wp);
                    if (iter == _nodes.end()) { return; }

                    _publish_unregister(iter);
                });
            });

    // Register all existing repositories
    vector<v2::config_registry_ptr> registries;
    config_registry::backend_t::bk_enumerate_registries(&registries, false);

    for (auto& rg : registries) {
        auto [iter, is_new] = _nodes.try_emplace(rg);

        if (is_new) {
            _init_registry_node(iter, rg.get());
        }

        _update_registry_structure(iter);
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
}

void config_context::_init_registry_node(
        registry_table_type::iterator node, config_registry* ptr)
{
    ptr->backend()->evt_structure_changed.add_weak(
            _monitor_anchor, [this, ptr](config_registry* rg) {
                assert(rg == ptr);

                post(*_ioc, [this, wrg = rg->weak_from_this()] {
                    auto iter = _nodes.find(wrg);
                    if (iter == _nodes.end()) { return; }

                    _update_registry_structure(iter);
                    _publish_registry_refresh(iter);
                });
            });
}

static auto CPPH_LOGGER()
{
    return detail::nglog();
}