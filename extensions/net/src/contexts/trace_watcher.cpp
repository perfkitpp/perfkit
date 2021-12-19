//
// Created by ki608 on 2021-11-28.
//

#include "trace_watcher.hpp"

#include <spdlog/spdlog.h>

#include "../utils.hpp"
#include "perfkit/common/algorithm.hxx"
#include "perfkit/common/macros.hxx"
#include "perfkit/common/template_utils.hxx"
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
        auto& watching = _watching;

        auto all = perfkit::tracer::all();
        std::vector<std::shared_ptr<tracer>> diffs;
        diffs.reserve(all.size());

        perfkit::set_difference2(
                all, watching, std::back_inserter(diffs),
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
            watching.assign(all.begin(), all.end());

            auto table = _signal_table.lock();
            perfkit::transform(
                    all, std::inserter(*table, table->end()),
                    [](decltype(all[0])& item) {
                        return std::make_pair(item->name(), std::weak_ptr{item});
                    });

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
             wtracer = std::move(tracer),
             life    = std::weak_ptr{_event_lifespan}]  //
            () mutable {
                auto locked = life.lock();
                if (not locked)
                    return;

                auto tracer = wtracer.lock();
                if (not tracer)
                    return;

                _dispatcher_fn(tracer, *buf);
            });
}

static void dump_trace(
        perfkit::tracer::trace const& v,
        perfkit::terminal::net::outgoing::traces::node_scheme* node)
{
    node->trace_key   = v.unique_id().value;
    node->name        = v.hierarchy.back();
    node->folded      = v.folded();
    node->subscribing = v.subscribing();

    auto visitor =
            [&](auto& value) {
                using namespace perfkit::terminal::net::outgoing;
                using type = std::remove_const_t<std::remove_reference_t<decltype(value)>>;

                if constexpr (std::is_same_v<type, nullptr_t>)
                {
                    node->value_type = TRACE_VALUE_NULLPTR;
                    node->value      = "";
                }
                else if constexpr (std::is_same_v<type, perfkit::tracer::clock_type::duration>)
                {
                    node->value_type = TRACE_VALUE_DURATION_USEC;
                    node->value      = std::to_string(
                                 std::chrono::duration_cast<std::chrono::microseconds>(value)
                                         .count());
                }
                else if constexpr (std::is_same_v<type, int64_t>)
                {
                    node->value_type = TRACE_VALUE_INTEGER;
                    node->value      = std::to_string(value);
                }
                else if constexpr (std::is_same_v<type, double>)
                {
                    node->value_type = TRACE_VALUE_FLOATING_POINT;
                    node->value      = std::to_string(value);
                }
                else if constexpr (std::is_same_v<type, std::string>)
                {
                    node->value_type = TRACE_VALUE_STRING;
                    node->value      = value;
                }
                else if constexpr (std::is_same_v<type, bool>)
                {
                    node->value_type = TRACE_VALUE_BOOLEAN;
                    node->value      = value ? "true" : "false";
                }
            };

    std::visit(visitor, v.data);
}

void perfkit::terminal::net::context::trace_watcher::_dispatcher_fn(
        const std::shared_ptr<perfkit::tracer>& tracer, const perfkit::tracer::fetched_traces& traces)
{
    stopwatch sw;
    CPPH_TRACE("dispatching {} traces from {}", traces.size(), tracer->name());

    outgoing::traces trc;
    trc.class_name = tracer->name();

    std::vector<decltype(&trc.root)> stack;
    std::vector<decltype(traces.data())> hierarchy;

    stack.reserve(10);
    hierarchy.reserve(10);

    stack.emplace_back(&trc.root);
    hierarchy.emplace_back(traces.data());

    // first case(root) is special
    ::dump_trace(traces[0], &trc.root);

    // build trace tree
    for (auto const& trace : make_iterable(traces.begin() + 1, traces.end()))
    {
        assert(stack.size() == hierarchy.size());

        auto key = trace.unique_id();
        if (auto [it, is_new] = _nodes.try_emplace(key); is_new)
        {
            auto* item   = &it->second;
            item->subscr = std::shared_ptr<std::atomic_bool>(
                    tracer, trace._bk_p_subscribed());
            item->fold = std::shared_ptr<std::atomic_bool>(
                    tracer, trace._bk_p_folded());
        }

        while (trace.parent_unique_order != hierarchy.back()->unique_order)
        {  // pop all non-relatives
            hierarchy.pop_back();
            stack.pop_back();

            assert(not stack.empty());
            assert(not hierarchy.empty());
        }

        auto parent = stack.back();
        hierarchy.push_back(&trace);
        stack.emplace_back(&parent->children.emplace_back());

        ::dump_trace(*hierarchy.back(), stack.back());
    }

    io->send(trc);
    CPPH_TRACE("elapsed: {} sec", sw.elapsed().count());
}

void perfkit::terminal::net::context::trace_watcher::signal(std::string_view class_name)
{
    auto lock = _signal_table.lock();

    auto it = lock->find(class_name);
    if (it == lock->end())
        return;

    auto tracer = it->second.lock();
    if (not tracer)
        return;

    CPPH_TRACE("trace signal to {}", class_name);
    tracer->request_fetch_data();
}

void perfkit::terminal::net::context::trace_watcher::tweak(
        uint64_t key, const bool* subscr, const bool* fold)
{
    std::optional<bool> osubs, ofold;
    if (subscr) { osubs = *subscr; }
    if (fold) { ofold = *fold; }

    io->dispatch(
            [this, key, osubs, ofold, wlife = std::weak_ptr{_event_lifespan}]  //
            {
                auto alive = wlife.lock();
                if (not alive)
                    return;

                auto it = _nodes.find({key});
                if (it == _nodes.end())
                    return;

                if (osubs)
                    if (auto psubs = it->second.subscr.lock())
                        *psubs = *osubs;

                if (ofold)
                    if (auto pfold = it->second.fold.lock())
                        *pfold = *ofold;
            });
}
