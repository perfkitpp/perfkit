//
// Created by ki608 on 2021-11-21.
//

#include "dispatcher.hpp"

#include <utility>

#include <asio/dispatch.hpp>
#include <perfkit/common/assert.hxx>

#include "basic_dispatcher_impl.hpp"
#include "server_mode_dispatcher.hpp"

perfkit::terminal::net::dispatcher::~dispatcher() = default;
perfkit::terminal::net::dispatcher::dispatcher(
        const perfkit::terminal::net::terminal_init_info& init_info)
{
    // TODO: add advanced options for terminal_init_info
    detail::basic_dispatcher_impl_init_info init0;
    init0.auth = init_info.auths;

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
        void const* userobj,
        void (*payload)(send_archive_type*, void const*))
{
    self->send(route, fence, userobj, payload);
}

perfkit::event<>& perfkit::terminal::net::dispatcher::on_no_connection()
{
    return self->on_no_connection;
}

perfkit::event<int>& perfkit::terminal::net::dispatcher::on_new_connection()
{
    return self->on_new_connection;
}

std::pair<int, int> perfkit::terminal::net::dispatcher::bandwidth_io() const noexcept
{
    return std::make_pair(self->in_rate(), self->out_rate());
}

void perfkit::terminal::net::dispatcher::launch()
{
    self->launch();
}

void perfkit::terminal::net::dispatcher::dispatch(perfkit::function<void()> fn)
{
    asio::dispatch(*self->io(), std::move(fn));
}
