//
// Created by ki608 on 2021-11-27.
//

#pragma once
#include "if_watcher.hpp"

namespace perfkit::terminal::net::context
{
class config_watcher : public if_watcher
{
   public:
    void update() override {}
};

}  // namespace perfkit::terminal::net::context
