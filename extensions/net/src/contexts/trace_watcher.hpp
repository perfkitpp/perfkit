//
// Created by ki608 on 2021-11-28.
//

#pragma once
#include <perfkit/common/thread/worker.hxx>
#include <perfkit/traces.h>

#include "if_watcher.hpp"

namespace perfkit::terminal::net::context {
class trace_watcher : public if_watcher
{
   public:
    void start() override;
    void stop() override;
    void update() override;

   public:
    void signal(std::string_view);

   private:
    void _monitor_once();

   private:
    std::shared_ptr<int> _event_lifespan;
};
}  // namespace perfkit::terminal::net::context
