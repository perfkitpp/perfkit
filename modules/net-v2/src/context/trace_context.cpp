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

#include "trace_context.hpp"

#include "perfkit/common/refl/object.hxx"
#include "perfkit/common/refl/rpc/rpc.hxx"
#include "perfkit/common/refl/rpc/service.hxx"
#include "perfkit/detail/tracer.hpp"

void perfkit::net::trace_context::build_service(perfkit::rpc::service_builder& target)
{
    target.route(message::service::trace_request_update, bind_front(&self_type::_rpc_request_update, this));
    target.route(message::service::trace_request_control, bind_front(&self_type::_rpc_request_control, this));
}

void perfkit::net::trace_context::start_monitoring(std::weak_ptr<void> anchor)
{
    _monitor_anchor = std::move(anchor);

    // Register tracer on_create event
    tracer::on_new_tracer().add_weak(
            _monitor_anchor, [this](tracer* p) {
                _host->post(&self_type::_register_tracer, this, p->shared_from_this());
            });

    // Register existing tracers
    {
        auto all_tracer = tracer::all();

        // Prevent memory reallocation from repetitive table insertion
        _tracers_by_id.reserve(all_tracer.size());

        for (auto& tr : all_tracer) { _register_tracer(std::move(tr)); }
    }
}

void perfkit::net::trace_context::stop_monitoring()
{
    // Registered callbacks will automatically be disposed when the anchor is released.
    _monitor_anchor = {};

    _tracers.clear();
    _tracers_by_id.clear();
}

void perfkit::net::trace_context::rpc_republish_all_tracers()
{
    auto fn_publish_all =
            [this] {
                for (auto& [k, v] : _tracers)
                    _publish_tracer_creation(*v);
            };

    _host->post_weak(_monitor_anchor, fn_publish_all);
}

void perfkit::net::trace_context::_register_tracer(std::shared_ptr<perfkit::tracer> tr)
{
    auto& pinfo = _tracers[tr];
    pinfo = std::make_shared<tracer_info_t>();
    pinfo->tracer_id = ++_uid_gen;
    pinfo->wref = tr;

    // Register ID mapping
    _tracers_by_id[pinfo->tracer_id] = pinfo;

    tr->on_destroy.add_weak(
            _monitor_anchor,
            [this](tracer* ptr) {
                // Unregister given tracer
                _host->post(&self_type::_unregister_tracer, this, ptr->weak_from_this());
            });

    tr->on_fetch.add(
            [this, winfo = weak_ptr{pinfo}](tracer::trace_fetch_proxy const& proxy) {
                auto info = winfo.lock();
                if (not info) { return; }

                auto pbuf = _trace_bufs.checkout();

                proxy.fetch_diff(pbuf.get(), info->_.fence);
                info->_.fence = proxy.fence();

                _host->post(
                        &self_type::_on_fetch, this,
                        winfo, std::move(pbuf), proxy.num_all_nodes());
            });

    _publish_tracer_creation(*pinfo);
}

void perfkit::net::trace_context::_publish_tracer_creation(
        const perfkit::net::trace_context::tracer_info_t& info)
{
    auto ptr = info.wref.lock();
    if (not ptr) { return; }

    message::tracer_descriptor_t tracer = {};
    tracer.tracer_id = info.tracer_id;
    tracer.epoch = std::chrono::duration_cast<std::chrono::milliseconds>(ptr->epoch().time_since_epoch());
    tracer.name = ptr->name();
    tracer.priority = ptr->order();

    message::notify::new_tracer(_host->rpc()).notify(tracer, _host->fn_admin_access());
}

void perfkit::net::trace_context::_unregister_tracer(std::weak_ptr<perfkit::tracer> wp)
{
    auto pair = find_ptr(_tracers, wp);
    if (not pair) { return; }

    auto pinfo = pair->second;
    _tracers.erase(wp);
    _tracers_by_id.erase(pinfo->tracer_id);

    message::notify::deleted_tracer(_host->rpc()).notify(pinfo->tracer_id);
}
