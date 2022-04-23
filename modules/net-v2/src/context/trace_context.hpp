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
#include <set>

#include "../net_terminal_adapter.hpp"
#include "cpph/memory/pool.hxx"
#include "cpph/refl/rpc/core.hxx"
#include "perfkit/detail/tracer.hpp"
#include "perfkit/extension/net/protocol.hpp"
#include "perfkit/fwd.hpp"

namespace perfkit::net {
class trace_context
{
   private:
    struct tracer_info_t {
        weak_ptr<tracer> wref;
        uint64_t tracer_id;

        vector<tracer::trace> traces;
        bool remote_up_to_date = false;

        // Stores local state.
        struct {
            size_t fence = 0;
        } _;
    };

   private:
    using self_type = trace_context;
    using tracer_info_table_type = std::map<weak_ptr<tracer>, shared_ptr<tracer_info_t>, std::owner_less<>>;
    using tracer_id_table_type = std::unordered_map<uint64_t, shared_ptr<tracer_info_t>>;

   private:
    if_net_terminal_adapter* _host;

    // Monitoring is only valid for this reference's lifetime.
    weak_ptr<void> _monitor_anchor;

    // List of registered tracers
    tracer_info_table_type _tracers;
    tracer_id_table_type _tracers_by_id;

    // Stores fetched trace lists temporarily.
    pool<tracer::fetched_traces> _trace_bufs;

    // Unique ID generator
    uint64_t _uid_gen = 0;

    // Reused message buffer
    vector<message::trace_update_t> _buf_updates;
    vector<message::trace_info_t> _buf_info;

   public:
    explicit trace_context(if_net_terminal_adapter* host) : _host(host) {}

   public:
    void build_service(rpc::service_builder& target);

    void start_monitoring(weak_ptr<void> anchor);
    void stop_monitoring();

    //! Possibly called from other thread.
    void rpc_republish_all_tracers();

   private:
    void _publish_tracer_creation(tracer_info_t const&);
    void _publish_tracer_id_list();
    void _register_tracer(shared_ptr<tracer>);
    void _unregister_tracer(weak_ptr<tracer> wp);
    void _on_fetch(
            weak_ptr<tracer_info_t> const&,
            pool_ptr<tracer::fetched_traces>&,
            size_t fence,
            size_t max_index);

   private:
    //! RPC handlers, may called from different thread ...
    void _rpc_request_update(uint64_t tracer_id);
    void _rpc_reset_cache(uint64_t tracer_id);
    void _rpc_request_control(uint64_t tracer_id, int index, message::service::trace_control_t const&);
};
}  // namespace perfkit::net
