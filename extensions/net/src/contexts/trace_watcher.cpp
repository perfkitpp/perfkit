//
// Created by ki608 on 2021-11-28.
//

#include "trace_watcher.hpp"

void perfkit::terminal::net::context::trace_watcher::start()
{
    if_watcher::start();
}

void perfkit::terminal::net::context::trace_watcher::stop()
{
    if_watcher::stop();
    io;
}

void perfkit::terminal::net::context::trace_watcher::update()
{
    if_watcher::update();
}
