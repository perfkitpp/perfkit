//
// Created by ki608 on 2021-11-21.
//

#include "dispatcher.hpp"

#include "basic_dispatcher_impl.hpp"

perfkit::terminal::net::dispatcher::~dispatcher() = default;
perfkit::terminal::net::dispatcher::dispatcher(
        perfkit::terminal::net::dispatcher::impl_ptr ptr)
        : self{std::move(ptr)} {}
