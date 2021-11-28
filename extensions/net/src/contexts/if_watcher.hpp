#pragma once
#include "../dispatcher.hpp"

namespace perfkit::terminal::net::context
{
class if_watcher
{
   public:
    virtual ~if_watcher() = default;

   public:
    // watcher calls for this when there's any change, thus update() call would do something.
    std::function<void()> notify_change;
    dispatcher* io = nullptr;

   public:
    virtual void start(){};
    virtual void update(){};
};
}  // namespace perfkit::terminal::net::context
