//
// Created by ki608 on 2021-11-27.
//

#pragma once
#include "if_watcher.hpp"
#include "perfkit/common/algorithm.hxx"
#include "perfkit/common/template_utils.hxx"
#include "perfkit/common/thread/worker.hxx"
#include "perfkit/common/timer.hxx"
#include "perfkit/detail/configs.hpp"

namespace perfkit::terminal::net::context
{
using namespace std::literals;

class config_watcher : public if_watcher
{
   public:
    void start() override
    {
        // launch watchdog thread
        _worker.repeat(CPPH_BIND(_watchdog_once));
        _has_update = true;
    }

    void stop() override
    {
        _worker.shutdown();
        _cache = {};
    }

    void update() override
    {
        if (not _has_update)
            return;

        if (not _min_interval())
            return;  // prevent too frequent update request

        // clear dirty flag
        _has_update = false;

        // check for new registries
        {
            auto regs     = perfkit::config_registry::bk_enumerate_registries();
            auto* watches = &_cache.regs;

            sort(regs, [](auto&& a, auto&& b)
                 { return a.owner_before(b); });

            std::vector<std::shared_ptr<config_registry>> diffs;
            diffs.reserve(regs.size());

            std::set_difference(
                    regs.begin(), regs.end(),
                    watches->begin(), watches->end(),
                    std::back_inserter(diffs),
                    [](auto&& a, auto&& b)
                    {
                        return ptr_equals(a, b);
                    });

            watches->assign(regs.begin(), regs.end());

            for (auto& diff : diffs)
            {
                _publish_registry(&*diff);
            }
        }

        // check for indivisual config's updates
    }

   private:
    void _watchdog_once()
    {
        auto has_update = perfkit::configs::wait_any_change(500ms, &_fence_value);
        has_update && (_has_update.store(true), notify_change(), 0);
    }

    void _publish_registry(config_registry* rg)
    {
        auto* ents = &_cache.entities;
        
    }

   private:
    thread::worker _worker;
    std::atomic_bool _has_update = false;
    uint64_t _fence_value        = 0;
    poll_timer _min_interval{50ms};

    struct _entity_context
    {
        std::weak_ptr<perfkit::detail::config_base> config;
        uint64_t last_flag = 0;
    };

    struct
    {
        std::vector<std::weak_ptr<perfkit::config_registry>> regs;
        std::vector<_entity_context> entities;
    } _cache;
};

}  // namespace perfkit::terminal::net::context
