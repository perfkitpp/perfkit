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

#include "cpph/refl/object.hxx"
#include "cpph/refl/rpc/rpc.hxx"
#include "cpph/refl/rpc/service.hxx"
#include "perfkit/detail/tracer.hpp"
#include "range/v3/view/map.hpp"

void perfkit::net::trace_context::build_service(perfkit::rpc::service_builder& target)
{
    target.route(message::service::trace_request_control, bind_front(&self_type::_rpc_request_control, this));
    target.route(message::service::trace_request_update, bind_front(&self_type::_rpc_request_update, this));
    target.route(message::service::trace_reset_cache, bind_front(&self_type::_rpc_reset_cache, this));
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
                for (auto& [k, v] : _tracers) {
                    v->remote_up_to_date = false;
                    _publish_tracer_creation(*v);
                }

                _publish_tracer_id_list();
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
                        winfo, std::move(pbuf),
                        proxy.fence(),
                        proxy.num_all_nodes());
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
    if (auto pair = find_ptr(_tracers, wp)) {
        auto pinfo = pair->second;
        _tracers.erase(wp);
        _tracers_by_id.erase(pinfo->tracer_id);
    }

    _publish_tracer_id_list();
}

void perfkit::net::trace_context::_rpc_request_update(uint64_t tracer_id)
{
    _host->post(
            [this, tracer_id] {
                auto info = find_ptr(_tracers_by_id, tracer_id);
                if (not info) { return; }

                auto tracer = info->second->wref.lock();
                if (not tracer) { return; }

                tracer->request_fetch_data();
            });
}

void perfkit::net::trace_context::_rpc_reset_cache(uint64_t tracer_id)
{
    _host->post(
            [this, tracer_id] {
                auto info = find_ptr(_tracers_by_id, tracer_id);
                if (not info) { return; }

                info->second->remote_up_to_date = false;
            });
}

void perfkit::net::trace_context::_rpc_request_control(
        uint64_t tracer_id, int index, const perfkit::net::message::service::trace_control_t& arg)
{
    _host->post(
            [this, tracer_id, index, arg] {
                auto info = find_ptr(_tracers_by_id, tracer_id);
                if (not info) { return; }

                try {
                    auto e = &info->second->traces.at(index);
                    if (arg.subscribe) e->subscribe(*arg.subscribe);
                    if (arg.fold) e->fold(*arg.fold);
                } catch (std::exception&) {
                }
            });
}

void perfkit::net::trace_context::_on_fetch(
        const weak_ptr<tracer_info_t>& winfo,
        pool_ptr<tracer::fetched_traces>& pbuf,
        size_t fence,
        size_t max_index)
{
    auto info = winfo.lock();
    if (not info) { return; }

    _buf_updates.clear();
    _buf_info.clear();

    auto* traces = &info->traces;
    bool const republish_all = not info->remote_up_to_date;
    size_t const latest_size = traces->size();

    //
    auto fn_apnd_msg_info =
            [this, tracer_id = info->tracer_id](tracer::trace const& e) {
                auto* m = &_buf_info.emplace_back();
                m->name = e.key;
                m->hash = e.hash;
                m->index = e.unique_order;
                m->parent_index = e.owner_node ? e.owner_node->unique_order : -1;
                m->owner_tracer_id = tracer_id;
            };

    auto fn_apnd_msg_update =
            [this](tracer::trace const& e) {
                auto* m = &_buf_updates.emplace_back();
                m->index = e.unique_order;
                m->occurrence_order = e.active_order;
                m->fence_value = e.fence;
                m->ref_subscr() = e.subscribing();
                m->ref_fold() = e.folded();
                m->payload = e.data;
            };

    for (auto& entity : *pbuf) {
        // Check if new entity was added
        if (latest_size < entity.unique_order + 1) {
            if (traces->size() <= entity.unique_order)
                traces->resize(entity.unique_order + 1);

            // Add new entity to publish target if it's not republish all mode
            if (not republish_all)
                fn_apnd_msg_info(entity);
        }

        // Add updated entity to publish target if it's not republish all mode.
        if (not republish_all)
            fn_apnd_msg_update(entity);

        // Apply update
        auto* e = &(*traces)[entity.unique_order];
        swap(*e, entity);
    }

    if (republish_all) {
        // Publish all entities again
        info->remote_up_to_date = true;

        // Iterate all traces
        for (auto& e : info->traces) {
            fn_apnd_msg_info(e);
            fn_apnd_msg_update(e);
        }
    }

    // Publish updates if there's any.
    if (not _buf_info.empty()) {
        message::notify::new_trace_node(_host->rpc())
                .notify(info->tracer_id, _buf_info, _host->fn_admin_access());
    }
    if (not _buf_updates.empty()) {
        message::notify::trace_node_update(_host->rpc())
                .notify(info->tracer_id, _buf_updates, _host->fn_admin_access());
    }
}

void perfkit::net::trace_context::_publish_tracer_id_list()
{
    vector<uint64_t> tracers;
    tracers.reserve(_tracers_by_id.size());
    copy(_tracers_by_id | ranges::views::keys, back_inserter(tracers));

    message::notify::validate_tracer_list(_host->rpc()).notify(tracers);
}
