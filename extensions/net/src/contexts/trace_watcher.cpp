//
// Created by ki608 on 2021-11-28.
//

#include "trace_watcher.hpp"

#include <spdlog/spdlog.h>

#include "../utils.hpp"
#include "perfkit/common/algorithm.hxx"
#include "perfkit/common/macros.hxx"
#include "perfkit/extension/net-internals/messages.hpp"

#define CPPH_LOGGER() perfkit::terminal::net::detail::nglog()

void perfkit::terminal::net::context::trace_watcher::start()
{
    if_watcher::start();
    _tmr_enumerate.invalidate();
    _event_lifespan = std::make_shared<nullptr_t>();
}

void perfkit::terminal::net::context::trace_watcher::stop()
{
    if_watcher::stop();
    _watching.clear();

    if (_event_lifespan)
        while (not _event_lifespan.unique())
            std::this_thread::yield();  // flush pending async oprs

    _event_lifespan.reset();
}

void perfkit::terminal::net::context::trace_watcher::update()
{
    if_watcher::update();

    if (_tmr_enumerate.check())
    {
        auto all = perfkit::tracer::all();
        std::vector<std::shared_ptr<tracer>> diffs;
        diffs.reserve(all.size());

        perfkit::set_difference2(
                all, _watching, std::back_inserter(diffs),
                [](auto&& a, auto&& b) { return a.owner_before(b); });

        if (not diffs.empty())
        {
            for (auto& diff : diffs)
            {
                diff->on_fetch +=
                        [this,
                         life   = std::weak_ptr{_event_lifespan},
                         tracer = std::weak_ptr{diff}]  //
                        (tracer::fetched_traces const& traces) {
                            if (auto _ = life.lock())
                            {
                                _dispatch_fetched_trace(tracer, traces);
                                return true;
                            }
                            else
                            {
                                return false;
                            }
                        };
            }

            CPPH_DEBUG("trace list changed. publishing {} tracer names ...", diffs.size());
            _watching.assign(all.begin(), all.end());

            outgoing::trace_class_list list;
            perfkit::transform(
                    all, std::front_inserter(list.content),
                    [](decltype(all[0])& s) {
                        return s->name();
                    });

            io->send(list);
        }
    }
}

void perfkit::terminal::net::context::trace_watcher::_dispatch_fetched_trace(
        std::weak_ptr<perfkit::tracer> tracer,
        perfkit::tracer::fetched_traces const& traces)
{
    auto buf = _pool_traces.checkout();
    buf->assign(traces.begin(), traces.end());

    io->dispatch(
            [this,
             buf     = std::move(buf),
             wtracer = tracer,
             life    = std::weak_ptr{_event_lifespan}]  //
            () mutable {
                auto locked = life.lock();
                if (not locked)
                    return;

                auto tracer = wtracer.lock();
                if (not tracer)
                    return;

                outgoing::traces trc;
                trc.class_name = tracer->name();

                std::vector<decltype(trc.root)*> stack;
                stack.emplace_back(&trc.root);

                // build trace tree
                for (auto trace : *buf)
                {
                    auto pnode       = stack.back();
                    pnode->name      = trace.hierarchy.back();
                    pnode->trace_key = trace.unique_id().value;

                    auto key = trace.unique_id();
                    if (auto [it, is_new] = _nodes.try_emplace(key); is_new)
                    {
                        auto* item   = &it->second;
                        item->subscr = std::shared_ptr<std::atomic_bool>(
                                tracer, trace._bk_p_subscribed());
                        item->fold = std::shared_ptr<std::atomic_bool>(
                                tracer, trace._bk_p_folded());
                    }
                }
            });
}
