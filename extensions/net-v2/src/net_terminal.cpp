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
// Created by ki608 on 2022-03-06.
//

#include "net_terminal.hpp"

#include <spdlog/spdlog.h>

#include "perfkit/common/functional.hxx"
#include "perfkit/common/refl/msgpack-rpc/asio.hxx"
#include "perfkit/configs.h"
#include "utils.hpp"

using namespace std::literals;

perfkit::net::terminal::terminal(perfkit::net::terminal_info info) noexcept
        : _info(std::move(info)), _rpc(_build_service(), _rpc_monitor)
{
    using namespace std::chrono;
    _session_info.epoch = duration_cast<milliseconds>(
                                  steady_clock::now().time_since_epoch())
                                  .count();
    _session_info.name        = _info.name;
    _session_info.description = _info.description;
    _session_info.num_cores   = std::thread::hardware_concurrency();

    // key string generation
    char hostname[65];
    gethostname(hostname, std::size(hostname));

    _session_info.hostname = hostname;
    _session_info.keystr
            = _session_info.hostname + ';'
            + std::to_string(_session_info.num_cores) + ';'
            + _session_info.name + ';'
            + std::to_string(_session_info.epoch);
}

void perfkit::net::terminal::_start_()
{
    detail::input_redirect(bind_front(&terminal::_on_char_buf, this));

    CPPH_INFO("Starting session [{}] ...", _info.name);
    _worker.repeat(bind_front(&terminal::_worker_func, this));

    // Post event for opening acceptor
    _event_proc.post(bind_front(&terminal::_open_acceptor, this));
}

void perfkit::net::terminal::_open_acceptor()
{
    using namespace asio::ip;
    tcp::endpoint ep{make_address(_info.bind_ip), _info.bind_port};
    CPPH_INFO("accept>> Binding acceptor to {}:{} ...", _info.bind_ip, _info.bind_port);

    _acceptor.open(ep.protocol());
    _acceptor.set_option(asio::socket_base::reuse_address{true});
    _acceptor.bind(ep);
    CPPH_INFO("accept>> Bind successful. Starting listening ...");

    msgpack::rpc::session_config config;
    config.timeout         = 10s;
    config.use_integer_key = true;

    msgpack::rpc::asio_ex::open_acceptor(_rpc, config, _acceptor);
    CPPH_INFO("accept>> Now listening ...");
}

void perfkit::net::terminal::_worker_func()
{
    try {
        _event_proc.run_for(250ms);
        _event_proc.restart();
    } catch (asio::system_error& ec) {
        CPPH_ERROR("Socket error! ({}): {}", ec.code().value(), ec.what());
        CPPH_ERROR("Sleep for 3 seconds before retry ...");

        std::this_thread::sleep_for(3s);
        _event_proc.post(bind_front(&terminal::_open_acceptor, this));
    }
}

perfkit::net::terminal::~terminal()
{
    // Send stop signal to worker, and close acceptor not to receive foreign connections
    _worker.stop();
    _acceptor.close();

    // Disconnect all remote clients
    _rpc.disconnect_all();

    // Wait all async
    _worker.join();
    _ioc.join();

    detail::input_rollback();
}

perfkit::msgpack::rpc::service_info perfkit::net::terminal::_build_service()
{
    using namespace message;

    msgpack::rpc::service_info service_desc{};
    service_desc
            .serve(service::suggest,
                   [this](service::suggest_result_t*, string const& content, int cursor) {
                       ;  // TODO
                   })
            .serve(service::invoke_command,
                   [this](auto&&, string const& content) {
                       this->push_command(content);
                   })
            .serve(service::fetch_tty,
                   [this](tty_output_t* out, int64_t fence) {
                       // Returns requested range of bytes
                       lock_guard _lc_{_tty_lock};
                       out->fence = _tty_fence;

                       fence      = std::clamp<int64_t>(fence, _tty_fence - _tty_buf.size(), _tty_fence);
                       auto begin = _tty_buf.end() - (_tty_fence - fence);
                       auto end   = _tty_buf.end();

                       out->content.assign(begin, end);
                   })
            .serve(service::heartbeat,
                   [this] { CPPH_INFO("HEARTBEAT!"); })
            .serve(service::session_info,
                   [this](perfkit::net::message::service::session_info_t* rv) {
                       *rv = _session_info;
                   })
            .serve(service::update_config_entity,
                   [this](config_entity_t const& entity) {
                       ;  // TODO
                   });

    return service_desc;
}

std::optional<std::string> perfkit::net::terminal::fetch_command(std::chrono::milliseconds timeout)
{
    return _pending_commands.try_pop(timeout);
}

void perfkit::net::terminal::push_command(std::string_view command)
{
    _pending_commands.emplace(command);
}

void perfkit::net::terminal::write(std::string_view str)
{
    detail::write(str.data(), str.size());
}

void perfkit::net::terminal::_on_char_buf(const char* data, size_t size)
{
    {
        lock_guard _lc_{_tty_lock};
        _tty_buf.rotate_append(data, data + size);
        _tty_fence += size;
    }

    _tty_obuf.access([&](message::tty_output_t& buf) {
        buf.content.assign(data, size);
        buf.fence = _tty_fence;
        message::notify::tty(_rpc).notify_all(buf);
    });
}

void perfkit::net::terminal::_on_session_list_change()
{
    auto const num_sessions = _rpc.session_count();
    CPPH_INFO("{} sessions are currently active.", num_sessions);

    bool const should_start_monitoring = num_sessions;
    bool const has_no_change           = should_start_monitoring == !!_session_active_state_anchor;

    if (has_no_change)
        return;

    if (should_start_monitoring) {
        CPPH_INFO("First session created ... Starting monitoring.");

        // Start monitoring
        _session_active_state_anchor = std::make_shared<nullptr_t>();
        weak_ptr anchor              = _session_active_state_anchor;

        configs::on_new_config_registry().add_weak(
                anchor, [this](config_registry* reg) {
                    auto fn_lazy_publish =
                            [this, wreg = reg->weak_from_this()] {
                                if (auto reg = wreg.lock())
                                    _config_publish_new_registry(reg);
                            };

                    asio::post(_event_proc, std::move(fn_lazy_publish));
                });

        tracer::on_new_tracer().add_weak(
                anchor, [this](tracer* tracer) {
                    ;  // TODO
                });
    } else {
        CPPH_INFO("Last session disposed ... Stopping monitoring.");
        _session_active_state_anchor = nullptr;
    }
}

void perfkit::net::terminal::_config_publish_new_registry(shared_ptr<config_registry> rg)
{
    CPPH_DEBUG("Publishing config registry: {}", rg->name());

    auto& all  = rg->bk_all();
    auto  key  = rg->name();
    auto  root = message::notify::config_category_t{};

    for (auto& [_, config] : all) {
        if (config->is_hidden())
            continue;  // dont' publish hidden ones

        auto  hierarchy = config->tokenized_display_key();
        auto* level     = &root;

        for (auto category : make_iterable(hierarchy.begin(), hierarchy.end() - 1)) {
            auto it = perfkit::find_if(level->subcategories,
                                       [&](auto&& s) { return s.name == category; });

            if (it == level->subcategories.end()) {
                level->subcategories.emplace_back();
                it       = --level->subcategories.end();
                it->name = category;
            }

            level = &*it;
        }

        auto fn_archive =
                [](string* v, nlohmann::json const& js) {
                    nlohmann::json::to_msgpack(js, nlohmann::detail::output_adapter<char>(*v));
                };
        auto* dst = &level->entities.emplace_back();
        dst->name = config->tokenized_display_key().back();
        fn_archive(&dst->initial_value, config->serialize());
        
        //
        //        auto to_msgpack =
        //        auto content = nlohmann::json::to_msgpack(config->serialize());
        //        dst->initial_value.assign(content.begin(), content.end());
        //        dst->opt_one_of =
        //        dst->config_key = config_key_t::hash(&*config).value;
        //
        //        _cache.confmap.try_emplace({dst->config_key}, config);
    }
    //
    //    io->send(message);
    //
    //    // Subscribe changes
    //    rg->on_update.add_weak(
    //            _watcher_lifecycle,
    //            [this](perfkit::config_registry*                          rg,
    //                   perfkit::array_view<perfkit::detail::config_base*> updates) {
    //                auto wptr = rg->weak_from_this();
    //
    //                // apply updates .. as long as wptr alive, pointers can be accessed safely
    //                auto buffer   = std::vector(updates.begin(), updates.end());
    //                auto function = perfkit::bind_front(&config_watcher::_on_update, this, wptr, std::move(buffer));
    //                asio::post(*_ioc, std::move(function));
    //
    //                notify_change();
    //            });
    //
    //    rg->on_destroy.add_weak(
    //            _watcher_lifecycle,
    //            [this](perfkit::config_registry* rg) {
    //                std::vector<int64_t> keys_all;
    //
    //                // TODO: retrive all keys from rg, and delete them from confmap
    //                auto keys = rg->bk_all()
    //                          | views::values
    //                          | views::transform([](auto& p) { return config_key_t::hash(&*p); })
    //                          | ranges::to_vector;
    //
    //                auto function = perfkit::bind_front(
    //                        &config_watcher::_on_unregister, this, std::move(keys));
    //                asio::post(*_ioc, std::move(function));
    //
    //                notify_change();
    //            });
}

void perfkit::net::terminal_monitor::on_new_session(
        const perfkit::msgpack::rpc::session_profile& profile) noexcept
{
    CPPH_INFO("session [{}]>> Connected", profile.peer_name);
    _owner->_event_proc.post(bind_front(&terminal::_on_session_list_change, _owner));
}

void perfkit::net::terminal_monitor::on_dispose_session(
        const perfkit::msgpack::rpc::session_profile& profile) noexcept
{
    CPPH_INFO("session [{}]>> Disconnecting ... ", profile.peer_name);
    _owner->_event_proc.post(bind_front(&terminal::_on_session_list_change, _owner));
}
