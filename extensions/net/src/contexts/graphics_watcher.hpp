//
// Created by ki608 on 2021-11-28.
//

#pragma once
#include "if_watcher.hpp"

namespace perfkit::terminal::net::context {
class graphics_watcher : public if_watcher
{
   public:
    void update() override {}
};
}  // namespace perfkit::terminal::net::context
