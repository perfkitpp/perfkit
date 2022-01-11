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

#include "trace_watcher.hpp"

#include <spdlog/spdlog.h>

#include "../utils.hpp"
#include "perfkit/common/algorithm/std.hxx"
#include "perfkit/common/macros.hxx"
#include "perfkit/common/template_utils.hxx"
#include "perfkit/common/thread/utility.hxx"
#include "perfkit/extension/net-internals/messages.hpp"

#define CPPH_LOGGER() perfkit::terminal::net::detail::nglog()

using namespace perfkit::terminal::net::context;

void trace_watcher::start()
{
    if_watcher::start();
    _tmr_enumerate.invalidate();
    _event_lifespan = perfkit::make_null();
    _tracers.lock()->clear();

    io->dispatch(
            perfkit::bind_front_weak(
                    _event_lifespan,
                    [this, all = perfkit::tracer::all()] {
                        auto table = _tracers.lock();

                        // initial event proc
                        for (auto& tracer : all)
                            _on_new_tracer_impl(&*table, &*tracer);

                        _publish_tracer_list(&*table);
                    }));

    perfkit::tracer::on_new_tracer()
            .add_weak(_event_lifespan, CPPH_BIND(_on_new_tracer));
}

void trace_watcher::stop()
{
    if_watcher::stop();

    perfkit::wait_pointer_unique(_event_lifespan);
    _event_lifespan.reset();

    _tracers.lock()->clear();
}

void trace_watcher::_dispatch_fetched_trace(
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

                _dispatcher_impl_on_io(tracer, *buf);
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

void trace_watcher::_dispatcher_impl_on_io(
        const std::shared_ptr<perfkit::tracer>& tracer, perfkit::tracer::fetched_traces& traces)
{
    stopwatch sw;
    CPPH_TRACE("dispatching {} traces from {}", traces.size(), tracer->name());
    perfkit::sort_messages_by_rule(traces);

    outgoing::traces trc;
    trc.class_name = tracer->name();

    std::vector<decltype(&trc.root)> stack;
    std::vector<perfkit::tracer::trace const*> hierarchy;

    stack.reserve(10);
    hierarchy.reserve(10);

    stack.emplace_back(&trc.root);
    hierarchy.emplace_back(traces.data());

    // first case(root) is special
    ::dump_trace(traces[0], &trc.root);
    trc.root.is_fresh = true;

    auto fence = traces[0].fence;

    // build trace tree
    for (auto const& trace : traces)
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

        if (&trace == &traces[0])
            continue;

        while (trace.owner_node != hierarchy.back()->self_node)
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
        stack.back()->is_fresh = fence == hierarchy.back()->fence;
    }

    io->send(trc);
    CPPH_TRACE("elapsed: {} sec", sw.elapsed().count());
}

void trace_watcher::signal(std::string_view class_name)
{
    auto lock = _tracers.lock();

    auto it = lock->find(class_name);
    if (it == lock->end())
        return;

    auto tracer = it->second.lock();
    if (not tracer)
        return;

    CPPH_TRACE("trace signal to {}", class_name);
    tracer->request_fetch_data();
}

void trace_watcher::tweak(
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

void trace_watcher::_on_new_tracer(perfkit::tracer* tracer)
{
    auto weak  = tracer->weak_from_this();
    auto table = _tracers.lock();

    _on_new_tracer_impl(&*table, tracer);
    _publish_tracer_list(&*table);
}

void trace_watcher::_on_new_tracer_impl(
        decltype(_tracers)::value_type* table, perfkit::tracer* tracer)
{
    // replae existing instance
    (*table)[tracer->name()] = tracer->shared_from_this();

    // register destroy event
    tracer->on_destroy
            .add_weak(_event_lifespan, CPPH_BIND(_on_destroy_tracer));

    // register update event
    tracer->on_fetch.add_weak(
            _event_lifespan,
            bind_front(&trace_watcher::_dispatch_fetched_trace,
                       this, tracer->weak_from_this()));
}

void trace_watcher::_on_destroy_tracer(perfkit::tracer* tracer)
{
    CPPH_DEBUG("destroying watch for tracer {} ...", tracer->name());
    // Erase from list
    _tracers.use(
            [&](decltype(_tracers)::value_type& v) {
                v.erase(tracer->name());
            });

    // queue GC
    io->post(perfkit::bind_front_weak(_event_lifespan, CPPH_BIND(_gc_nodes_impl_on_io)));
    CPPH_DEBUG("destroying watch for tracer {} ... done.", tracer->name());
}

void trace_watcher::_publish_tracer_list(decltype(_tracers)::value_type* table)
{
    outgoing::trace_class_list message;

    for (auto [name, wptr] : *table)
    {
        auto sptr = wptr.lock();
        if (not sptr)
        {
            CPPH_WARN("tracer {} expired incorrectly!", name);
            continue;
        }

        auto arg    = &message.content.emplace_front();
        arg->first  = name;
        arg->second = perfkit::hasher::fnv1a_64(&*sptr);
    }

    io->send(message);
}

void trace_watcher::_gc_nodes_impl_on_io()
{
    perfkit::erase_if_each(_nodes, [](auto&& v) { return v.second.fold.expired(); });
}
