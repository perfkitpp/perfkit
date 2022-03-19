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

#include <perfkit/common/hasher.hxx>
#include <perfkit/common/macros.hxx>
#include <perfkit/extension/net/protocol.hpp>
#include <perfkit/fwd.hpp>

#include "../net_terminal_adapter.hpp"

namespace perfkit::net {
class config_context
{
    using self_t = config_context;

   private:
    CPPH_UNIQUE_KEY_TYPE(config_key_t);

    struct config_registry_context_t
    {
        std::unordered_set<config_key_t> associated_keys;
    };

    using registry_context_table_t = std::map<string, config_registry_context_t, std::less<>>;
    using config_key_table         = std::unordered_map<config_key_t, weak_ptr<detail::config_base>>;

   private:
    if_net_terminal_adapter* _host;

    asio::io_context*        _event_proc{_host->event_proc()};
    msgpack::rpc::context*   _rpc{_host->rpc()};

    registry_context_table_t _config_registries;
    config_key_table         _config_instances;

    weak_ptr<void>           _monitor_anchor;

    message::config_entity_update_t
            _buf_payload;

   public:
    explicit config_context(if_net_terminal_adapter* host) : _host(host) {}

   public:
    void start_monitoring(weak_ptr<void> anchor);
    void stop_monitoring();

   public:
    void rpc_republish_all_registries();
    void rpc_update_request(message::config_entity_update_t const& content);

   private:
    void _publish_new_registry(shared_ptr<config_registry>);
    void _handle_registry_destruction(string const& name);
    void _handle_update(config_registry*, vector<detail::config_base*> const&);
};
}  // namespace perfkit::net
