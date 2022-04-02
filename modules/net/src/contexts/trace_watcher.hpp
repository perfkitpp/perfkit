// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

//
// Created by ki608 on 2021-11-28.
//

#pragma once
#include "if_watcher.hpp"
#include "perfkit/common/memory/pool.hxx"
#include "perfkit/common/thread/locked.hxx"
#include "perfkit/common/thread/worker.hxx"
#include "perfkit/common/timer.hxx"
#include "perfkit/traces.h"

namespace perfkit::terminal::net::context {
using namespace std::literals;

class trace_watcher : public if_watcher
{
   private:
    struct _trace_node {
        std::weak_ptr<std::atomic_bool> subscr;
        std::weak_ptr<std::atomic_bool> fold;
    };

   private:
    perfkit::shared_null                                              _event_lifespan;
    locked<std::map<std::string, std::weak_ptr<tracer>, std::less<>>> _tracers;

    pool<perfkit::tracer::fetched_traces>                             _pool_traces;

    std::unordered_map<trace_key_t, _trace_node>                      _nodes;
    poll_timer                                                        _tmr_enumerate{1s};

   public:
    void start() override;
    void stop() override;

   public:
    void signal(std::string_view);
    void tweak(uint64_t key, bool const* subscr, bool const* fold);

   private:
    void _dispatch_fetched_trace(std::weak_ptr<perfkit::tracer> tracer, tracer::fetched_traces const&);
    void _dispatcher_impl_on_io(const std::shared_ptr<perfkit::tracer>& tracer, tracer::fetched_traces& traces);

    void _on_new_tracer(perfkit::tracer* tracer);
    void _on_new_tracer_impl(decltype(_tracers)::value_type*, perfkit::tracer* tracer);
    void _publish_tracer_list(decltype(_tracers)::value_type*);

    void _on_destroy_tracer(perfkit::tracer* tracer);

    void _gc_nodes_impl_on_io();
};
}  // namespace perfkit::terminal::net::context
