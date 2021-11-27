//
// Created by ki608 on 2021-11-21.
//

#include "dispatcher.hpp"

#include <utility>

#include <perfkit/common/assert.hxx>

#include "basic_dispatcher_impl.hpp"
#include "server_mode_dispatcher.hpp"

perfkit::terminal::net::dispatcher::~dispatcher() = default;
perfkit::terminal::net::dispatcher::dispatcher(
        const perfkit::terminal::net::terminal_init_info& init_info)
{
    // TODO: add advanced options for terminal_init_info
    detail::basic_dispatcher_impl_init_info init0;

    if (init_info._mode == operation_mode::independent_server)
    {
        detail::server_mode_dispatcher_init_info init1;
        init1.bind_port = init_info._port_param;
        init1.bind_addr = init_info._string_param;

        self = std::make_unique<detail::server_mode_dispatcher>(init0, init1);
    }
    else if (init_info._mode == operation_mode::relay_server_provider)
    {
        UNIMPLEMENTED();
    }
    else
    {
        throw std::logic_error{"specify valid terminal operation mode!"};
    }

    self->launch();
}

void perfkit::terminal::net::dispatcher::_register_recv(
        std::string route,
        std::function<bool(const recv_archive_type&)> fn)
{
    self->register_recv(std::move(route), std::move(fn));
}

void perfkit::terminal::net::dispatcher::_send(
        std::string_view route,
        int64_t fence,
        void* userobj,
        void (*payload)(send_archive_type*, void*))
{
    self->send(route, fence, userobj, payload);
}
