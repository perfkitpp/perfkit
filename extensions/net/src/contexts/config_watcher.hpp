//
// Created by ki608 on 2021-11-27.
//

#pragma once

#include "if_watcher.hpp"
#include "perfkit/common/hasher.hxx"
#include "perfkit/common/thread/worker.hxx"
#include "perfkit/common/timer.hxx"
#include "perfkit/detail/configs.hpp"
#include "perfkit/extension/net-internals/messages.hpp"

namespace perfkit::terminal::net::context
{
using namespace std::literals;

using config_key_t = basic_key<class LABEL_config_key_t>;

class config_watcher : public if_watcher
{
   public:
    void start() override;
    void stop() override;
    void update() override;

    void update_entity(uint64_t key, nlohmann::json&& value);

   private:
    void _watchdog_once();
    void _publish_registry(config_registry* rg);

   private:
    thread::worker _worker;
    std::atomic_bool _has_update = false;
    uint64_t _fence_value        = 0;
    poll_timer _min_interval{50ms};
    spinlock _mtx_entities;

    struct _entity_context
    {
        config_key_t id;
        std::string_view class_name;
        std::weak_ptr<perfkit::detail::config_base> config;
        uint64_t update_fence = 0;
    };

    struct _cache_type
    {
        std::vector<std::weak_ptr<perfkit::config_registry>> regs;
        std::vector<_entity_context> entities;
    } _cache;
};

}  // namespace perfkit::terminal::net::context
