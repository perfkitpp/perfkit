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
// Created by ki608 on 2021-11-27.
//

#include "config_watcher.hpp"

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "../utils.hpp"
#include "perfkit/common/algorithm/std.hxx"
#include "perfkit/common/template_utils.hxx"
#include "perfkit/detail/base.hpp"
//
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#define CPPH_LOGGER() detail::nglog()

using namespace perfkit::terminal::net::context;
namespace views                   = ranges::views;

config_watcher::config_watcher()  = default;
config_watcher::~config_watcher() = default;

void config_watcher::update()
{
    _ioc->restart();
    _ioc->run();
}

void config_watcher::_publish_registry(perfkit::config_registry* rg)
{
    outgoing::new_config_class message;

    auto&                      all = rg->bk_all();
    message.key                    = rg->name();

    for (auto& [_, config] : all) {
        if (config->is_hidden())
            continue;  // dont' publish hidden ones

        auto  hierarchy = config->tokenized_display_key();
        auto* level     = &message.root;

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

        auto* dst       = &level->entities.emplace_back();
        dst->name       = config->tokenized_display_key().back();
        dst->value      = config->serialize();
        dst->metadata   = config->attribute();
        dst->config_key = config_key_t::hash(&*config).value;

        _cache.confmap.try_emplace({dst->config_key}, config);
    }

    io->send(message);

    // Subscribe changes
    rg->on_update.add_weak(
            _watcher_lifecycle,
            [this](perfkit::config_registry*                          rg,
                   perfkit::array_view<perfkit::detail::config_base*> updates) {
                auto wptr = rg->weak_from_this();

                // apply updates .. as long as wptr alive, pointers can be accessed safely
                auto buffer   = std::vector(updates.begin(), updates.end());
                auto function = perfkit::bind_front(&config_watcher::_on_update, this, wptr, std::move(buffer));
                asio::post(*_ioc, std::move(function));

                notify_change();
            });

    rg->on_destroy.add_weak(
            _watcher_lifecycle,
            [this](perfkit::config_registry* rg) {
                std::vector<int64_t> keys_all;

                // TODO: retrive all keys from rg, and delete them from confmap
                auto keys = rg->bk_all()
                          | views::values
                          | views::transform([](auto& p) { return config_key_t::hash(&*p); })
                          | ranges::to_vector;

                auto function = perfkit::bind_front(
                        &config_watcher::_on_unregister, this, std::move(keys));
                asio::post(*_ioc, std::move(function));

                notify_change();
            });
}

void config_watcher::stop()
{
    _worker.shutdown();
    _cache = {};
}

void config_watcher::start()
{
    // launch watchdog thread
    _ioc.reset(), _ioc = std::make_unique<asio::io_context>();
    _watcher_lifecycle = perfkit::make_null();

    perfkit::configs::on_new_config_registry().add_weak(
            _watcher_lifecycle,
            [this](perfkit::config_registry* ptr) {
                asio::post(
                        *_ioc,
                        [this, wptr = ptr->weak_from_this()] {
                            if (auto rg = wptr.lock()) {
                                _publish_registry(&*rg);
                            }
                        });

                notify_change();
            });

    // Register initial
    asio::post(
            *_ioc, [this] {
                auto all_regs = perfkit::config_registry::bk_enumerate_registries(true);
                for (auto& reg : all_regs) { _publish_registry(&*reg); }
            });
}

void config_watcher::update_entity(uint64_t key, nlohmann::json&& value)
{
    auto function_in_main_thread =
            [this, key, value = std::move(value)]() mutable {
                auto it = _cache.confmap.find(config_key_t{key});
                if (it == _cache.confmap.end()) { return; }

                auto conf = it->second.lock();
                if (not conf) {
                    CPPH_DEBUG("Ghost entity found for key {}", key);
                    _cache.confmap.erase(it);
                    return;
                }

                conf->request_modify(std::move(value));
            };

    asio::post(*_ioc, std::move(function_in_main_thread));
    notify_change();
}

void config_watcher::_on_update(
        std::weak_ptr<config_registry>             wrg,
        std::vector<perfkit::detail::config_base*> args)
{
    auto rg = wrg.lock();
    if (not rg) { return; }

    // check for indivisual config's updates
    outgoing::config_entity message;
    message.class_key = rg->name();

    for (auto arg : args) {
        auto* p       = &message.content.emplace_front();
        p->config_key = config_key_t::hash(&*arg).value;
        p->value      = arg->serialize();
    }

    io->send(message);
}

void config_watcher::_on_unregister(std::vector<config_key_t> keys)
{
    for (auto key : keys) { _cache.confmap.erase(key); }
}
