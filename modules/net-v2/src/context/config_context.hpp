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

#pragma once
#include <unordered_map>
#include <unordered_set>

#include <asio/steady_timer.hpp>
#include <cpph/hasher.hxx>
#include <cpph/macros.hxx>
#include <perfkit/extension/net/protocol.hpp>
#include <perfkit/fwd.hpp>

#include "../net_terminal_adapter.hpp"

namespace perfkit::net {
class config_context
{
    struct config_node_type {
        weak_ptr<config_base> config;
    };

    struct registry_node_type {
        message::notify::config_category_t cat_root;
    };

   private:
    using self_type = config_context;
    using registry_table_type = map<weak_ptr<config_registry>, registry_node_type, std::owner_less<>>;
    using config_table_type = std::unordered_map<uint64_t, config_node_type>;

   private:
    CPPH_UNIQUE_KEY_TYPE(config_key_t);

   private:
    if_net_terminal_adapter* _host;

    asio::io_context* _ioc{_host->event_proc()};
    rpc::session_group* _rpc{_host->rpc()};

    registry_table_type _nodes;
    std::unordered_map<uint64_t, weak_ptr<config_base>> _inv_mapping;
    weak_ptr<void> _monitor_anchor;

    std::unordered_map<uint64_t, void*> _tmp_lut;

   public:
    explicit config_context(if_net_terminal_adapter* host) : _host(host) {}

   public:
    void start_monitoring(weak_ptr<void> anchor);
    void stop_monitoring();

   public:
    void rpc_republish_all_registries();
    void rpc_update_request(message::config_entity_update_t& content);

   private:
    void _init_registry_node(registry_table_type::iterator, config_registry* ptr);
    void _update_registry_structure(config_registry* rg, registry_table_type::iterator const&);
    void _publish_registry_refresh(registry_table_type::iterator const& node);
    void _publish_unregister(registry_table_type::iterator node);
    void _publish_updates(shared_ptr<config_registry>, vector<shared_ptr<config_base>> const& update_list);
    void _gc_mappings();
};
}  // namespace perfkit::net
