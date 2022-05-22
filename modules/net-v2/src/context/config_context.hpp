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
    using self_t = config_context;

   private:
    CPPH_UNIQUE_KEY_TYPE(config_key_t);

   private:
    if_net_terminal_adapter* _host;

    asio::io_context* _event_proc{_host->event_proc()};
    rpc::session_group* _rpc{_host->rpc()};

    weak_ptr<void> _monitor_anchor;

    //
    asio::steady_timer _lazy_update_publish{*_event_proc};
    std::vector<weak_ptr<v2::config_base>> _pending_updates;
    message::config_entity_update_t _buf_payload;

   public:
    explicit config_context(if_net_terminal_adapter* host) : _host(host) {}

   public:
    void start_monitoring(weak_ptr<void> anchor);
    void stop_monitoring() {}

   public:
    void rpc_republish_all_registries() {}
    void rpc_update_request(message::config_entity_update_t const& content) {}
};
}  // namespace perfkit::net
