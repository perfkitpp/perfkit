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
   public:
    void start() override;
    void stop() override;
    void update() override;

   public:
    void signal(std::string_view);
    void tweak(uint64_t key, bool const* subscr, bool const* fold);

   private:
    void _dispatch_fetched_trace(std::weak_ptr<perfkit::tracer> tracer, tracer::fetched_traces const&);
    void _dispatcher_fn(const std::shared_ptr<perfkit::tracer>& tracer, tracer::fetched_traces& traces);

   private:
    struct _trace_node
    {
        std::weak_ptr<std::atomic_bool> subscr;
        std::weak_ptr<std::atomic_bool> fold;
    };

   private:
    std::shared_ptr<nullptr_t> _event_lifespan;
    std::vector<std::weak_ptr<tracer>> _watching;
    locked<std::map<std::string, std::weak_ptr<tracer>, std::less<>>> _signal_table;

    pool<perfkit::tracer::fetched_traces> _pool_traces;

    std::unordered_map<trace_key_t, _trace_node> _nodes;
    poll_timer _tmr_enumerate{3s};
};
}  // namespace perfkit::terminal::net::context
