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

#include <spdlog/spdlog.h>

#include "../utils.hpp"
#include "perfkit/common/algorithm.hxx"
#include "perfkit/common/template_utils.hxx"
#include "perfkit/detail/base.hpp"
//
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#define CPPH_LOGGER() detail::nglog()

using namespace perfkit::terminal::net::context;

config_watcher::config_watcher()  = default;
config_watcher::~config_watcher() = default;

void config_watcher::update()
{
    _ioc->restart();
    _ioc->run();

    // check for indivisual config's updates
    std::map<std::string_view, outgoing::config_entity> updates;

    for (auto& [_, message] : updates)
    {
        io->send(message);
    }
}

void config_watcher::_publish_registry(perfkit::config_registry* rg)
{
    outgoing::new_config_class message;

    auto& all   = rg->bk_all();
    message.key = rg->name();

    for (auto& [_, config] : all)
    {
        if (config->is_hidden())
            continue;  // dont' publish hidden ones

        auto hierarchy = config->tokenized_display_key();
        auto* level    = &message.root;

        for (auto category : make_iterable(hierarchy.begin(), hierarchy.end() - 1))
        {
            auto it = perfkit::find_if(level->subcategories,
                                       [&](auto&& s) { return s.name == category; });

            if (it == level->subcategories.end())
            {
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
        dst->config_key = config_key_t::hash(&*config);

        _cache.confmap.try_emplace({dst->config_key}, config);
    }

    io->send(message);

    // Subscribe changes
    rg->on_update.add_auto_expire(
            _watcher_lifecycle,
            [this](perfkit::config_registry* rg,
                   perfkit::array_view<perfkit::detail::config_base*> updates) {
                auto wptr = rg->weak_from_this();

                // TODO: apply updates .. as long as wptr alive, pointers can be accessed safely
            });

    rg->on_destroy.add_auto_expire(
            _watcher_lifecycle,
            [this](perfkit::config_registry* rg) {
                std::vector<int64_t> keys_all;

                // TODO: retrive all keys from rg, and delete them from confmap
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
    _ioc = std::make_unique<asio::io_context>();

    perfkit::configs::on_new_config_registry().add_auto_expire(
            _watcher_lifecycle,
            [this](perfkit::config_registry* ptr) {
                asio::post(
                        *_ioc,
                        [this, wptr = ptr->weak_from_this()] {
                            if (auto rg = wptr.lock())
                            {
                                _publish_registry(&*rg);
                            }
                        });

                notify_change();
            });
}

void config_watcher::update_entity(uint64_t key, nlohmann::json&& value)
{
}
