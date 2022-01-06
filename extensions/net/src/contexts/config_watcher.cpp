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

#define CPPH_LOGGER() detail::nglog()

using namespace perfkit::terminal::net::context;

void config_watcher::update()
{
    if (not _min_interval.check())
        return;  // prevent too frequent update request

    // check for new registries
    if (_tmr_config_registry.check())
    {
        auto regs     = perfkit::config_registry::bk_enumerate_registries(true);
        auto* watches = &_cache.regs;

        CPPH_TRACE("watching: {} entities", watches->size());
        CPPH_TRACE("enumerated: {} entities", regs.size());

        // discard obsolete registries
        auto it_erase = perfkit::remove_if(*watches, [](auto&& e) { return e.expired(); });

        if (it_erase != watches->end())
        {
            CPPH_DEBUG("{} registries disposed from last update", watches->end() - it_erase);
            watches->erase(it_erase, watches->end());
        }

        sort(regs, [](auto&& a, auto&& b) { return a.owner_before(b); });
        sort(*watches, [](auto&& a, auto&& b) { return a.owner_before(b); });

        std::vector<std::shared_ptr<config_registry>> diffs;
        diffs.reserve(regs.size());

        std::set_difference(
                regs.begin(), regs.end(),
                watches->begin(), watches->end(),
                std::back_inserter(diffs),
                [](auto&& a, auto&& b) {
                    return a.owner_before(b);
                });

        watches->assign(regs.begin(), regs.end());

        for (auto& diff : diffs)
        {
            CPPH_DEBUG("publishing new config registry {}", diff->name());
            _publish_registry(&*diff);
        }

        CPPH_TRACE("{} config registries are newly published.", diffs.size());
    }

    // check for indivisual config's updates
    if (_has_update)
    {
        _has_update = false;

        std::lock_guard lc{_mtx_entities};
        auto& ents = _cache.entities;

        std::map<std::string_view, outgoing::config_entity> updates;

        for (auto it = ents.begin(); it != ents.end();)
        {
            if (auto config = it->config.lock(); not config)
            {  // config is expired.
                *it = ents.back();
                ents.pop_back();
            }
            else
            {
                if (it->update_fence != config->num_modified())
                {
                    auto [it_msg, is_new] = updates.try_emplace(it->class_name);
                    auto* dst             = &it_msg->second;

                    if (is_new)
                    {
                        dst->class_key = it->class_name;
                    }

                    auto* elem       = &dst->content.emplace_front();
                    elem->value      = config->serialize();
                    elem->config_key = it->id.value;

                    it->update_fence = config->num_modified();
                }

                ++it;
            }
        }

        for (auto& [_, message] : updates)
        {
            io->send(message);
        }
    }
}

void config_watcher::_watchdog_once()
{
    auto has_update = perfkit::configs::wait_any_change(500ms, &_fence_value);
    has_update && (_has_update.store(true), notify_change(), 0);
}

void config_watcher::_publish_registry(perfkit::config_registry* rg)
{
    outgoing::new_config_class message;
    std::lock_guard lc{_mtx_entities};

    auto* ents = &_cache.entities;
    auto& all  = rg->bk_all();

    message.key = rg->name();
    ents->reserve(ents->size() + all.size());

    for (auto& [_, config] : all)
    {
        if (config->is_hidden())
            continue;  // dont' publish hidden ones

        auto* entity         = &ents->emplace_back();
        entity->class_name   = rg->name();
        entity->id           = config_key_t::create(&*config);
        entity->config       = config;
        entity->update_fence = config->num_modified();

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
        dst->config_key = entity->id.value;
    }

    io->send(message);

    // puts(nlohmann::json{message}.dump(2).c_str());
}

void config_watcher::stop()
{
    _worker.shutdown();
    _cache = {};
}

void config_watcher::start()
{
    // launch watchdog thread
    _worker.repeat(CPPH_BIND(_watchdog_once));
    _tmr_config_registry.invalidate();
    _has_update = true;
}

void config_watcher::update_entity(
        uint64_t key, nlohmann::json&& value)
{
    std::lock_guard lc{_mtx_entities};
    auto& ents = _cache.entities;

    auto it = perfkit::find_if(ents, [&](auto&& ent) { return ent.id.value == key; });
    if (it == ents.end())
        return;

    if (auto config = it->config.lock())
    {
        CPPH_TRACE("updating config entity {}:{}", it->class_name, config->display_key());
        config->request_modify(std::move(value));
    }
}
