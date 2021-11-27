//
// Created by ki608 on 2021-11-21.
//

#include "dispatcher.hpp"

#include <perfkit/common/assert.hxx>

#include "basic_dispatcher_impl.hpp"

perfkit::terminal::net::dispatcher::~dispatcher() = default;
perfkit::terminal::net::dispatcher::dispatcher(
        const perfkit::terminal::net::terminal_init_info& init_info)
{
    // TODO: create appropriate implementation class from init_info
}

void perfkit::terminal::net::dispatcher::_register_recv(
        std::string route,
        std::function<bool(const recv_archive_type&)> fn)
{
    self->register_recv(route, std::move(fn));
}

void perfkit::terminal::net::dispatcher::_send(
        std::string_view route,
        int64_t fence,
        void* userobj,
        void (*payload)(send_archive_type*, void*))
{
    self->send(route, fence, userobj, payload);
}
