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

#include <range/v3/all.hpp>

//
#include <spdlog/spdlog.h>

#include "cpph/refl/types/set.hxx"
#include "cpph/thread/spinlock.hxx"
#include "perfkit/configs.h"
#include "perfkit/detail/base.hpp"
#include "perfkit/detail/configs-v2-backend.hpp"
#include "perfkit/detail/logging.hpp"

using namespace cpph;
namespace vw = ranges::view;

namespace perfkit {
static auto CPPH_LOGGER() { return perfkit::glog(); }

static event<string> logger_registered;

logger_ptr share_logger(string const& name)
{
    static spinlock mtx;
    auto ptr = spdlog::get(name);
    bool newly_added = false;

    {
        std::lock_guard _{mtx};

        if (not ptr) {
            ptr = spdlog::default_logger()->clone(name);
            try {
                spdlog::register_logger(ptr);
                newly_added = true;
            } catch (...) {
                ptr = spdlog::get(name);
                assert(ptr);
            }
        }
    }

    if (newly_added) {
        logger_registered.invoke(name);
    }

    return ptr;
}

PERFKIT_CFG_CLASS(log_config_base)
{
    PERFKIT_CFG(reload_all, false).transient();
    PERFKIT_CFG(active_loggers, set<string>{}).hide();
};

PERFKIT_CFG_CLASS(log_config_entity)
{
    PERFKIT_CFG(level, "none").one_of({"trace", "debug", "info", "warning", "error", "critical", "off", "none"});
};

class log_level_control : public enable_shared_from_this<log_level_control>
{
    log_config_base _cfg;
    map<string, log_config_entity, less<>> _entities;

   public:
    explicit log_level_control(string name) : _cfg{log_config_base::create(move(name))}
    {
    }

   public:
    void start()
    {
        // Perform initial update to load values
        _cfg->update();

        logger_registered.add_weak(
                weak_from_this(),
                [this](string const& s) {
                    auto v = _cfg.active_loggers.value();
                    v.insert(s);
                    _cfg.active_loggers.commit(move(v));
                });

        reload_all();
    }

    void tick()
    {
        if (not _cfg->update()) { return; }

        if (*_cfg.reload_all) {
            _cfg.reload_all.set(false);
            this->reload_all();
            _cfg->update();
        }

        if (_cfg.active_loggers.check_update()) {
            list<string> added;
            set_difference2(*_cfg.active_loggers, vw::keys(_entities), back_inserter(added));

            for (auto& key : added) {
                auto& entity = _entities[key];
                assert(not entity.valid());

                auto entity_str = key;
                if (key.empty()) { entity_str += "__default"; }

                entity = log_config_entity::create(_cfg, move(entity_str));
                entity->touch();
            }

            if (not added.empty()) {
                _cfg->update();  // Let added nodes to load configs ..
            }

            for (auto& key : added) {
                auto& e = _entities[key];
                e.level.commit(*e.level);
            }

            CPPH_INFO("{} loggers loaded", added.size());
        }

        for (auto& [key, entity] : _entities | vw::filter([](auto&& r) { return r.second.level.check_update(); })) {
            logger_ptr logger;
            if (key.empty()) {
                logger = spdlog::default_logger();
            } else {
                logger = spdlog::get(key);
            }

            if (not logger)
                continue;

            if (*entity.level == "none") {
                auto levelstr = spdlog::level::to_string_view(logger->level());
                entity.level.set(string{levelstr.begin(), levelstr.end()});
            } else {
                logger->set_level(spdlog::level::from_str(*entity.level));
            }
        }
    }

    void reload_all()
    {
        set<string> all_loggers = *_cfg.active_loggers;

        spdlog::details::registry::instance().apply_all([&](logger_ptr const& ptr) {
            all_loggers.insert(ptr->name());
        });

        _cfg.active_loggers.commit(move(all_loggers));
    }
};

auto create_log_monitor(string name) -> shared_ptr<log_level_control>
{
    auto monitor = make_shared<log_level_control>(move(name));
    return monitor->start(), monitor->tick(), monitor;
}

void tick_log_monitor(log_level_control& l)
{
    l.tick();
}

void reload_log_monitor(log_level_control& l)
{
    l.reload_all();
}

}  // namespace perfkit
