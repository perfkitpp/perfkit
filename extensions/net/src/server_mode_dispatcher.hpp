
// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

//
// Created by ki608 on 2021-11-21.
//

#pragma once
#include <memory>

#include "basic_dispatcher_impl.hpp"

namespace perfkit::terminal::net::detail {
struct server_mode_dispatcher_init_info
{
    std::string bind_addr = "0.0.0.0";
    uint16_t bind_port    = 0;
};

class server_mode_dispatcher : public basic_dispatcher_impl
{
   public:
    using init_info = server_mode_dispatcher_init_info;
    server_mode_dispatcher(
            basic_dispatcher_impl::init_info const& base, init_info const& init)
            : basic_dispatcher_impl(base),
              _init(init) {}

    ~server_mode_dispatcher() override
    {
        cleanup();
    }

   protected:
    void refresh() override
    {
        if (_acceptor && _acceptor->is_open())
            _acceptor->close();  // just try close.

        _acceptor = std::make_unique<tcp::acceptor>(*io());
        tcp::endpoint endpoint{asio::ip::address::from_string(_init.bind_addr), _init.bind_port};

        _acceptor->open(endpoint.protocol());
        _acceptor->set_option(tcp::acceptor::reuse_address{true});
        _acceptor->bind(endpoint);
        _acceptor->listen();

        CPPH_INFO("opening acceptor for {}:{} ...", _init.bind_addr, _init.bind_port);

        struct _accept_fn
        {
            server_mode_dispatcher* self;
            std::unique_ptr<tcp::socket> sock;

            void operator()(asio::error_code const& ec)
            {
                auto CPPH_LOGGER = [this] { return self->CPPH_LOGGER(); };

                if (ec)
                {
                    if (ec == asio::error::operation_aborted)
                    {
                        CPPH_INFO("aborting accept opeartion");
                        return;
                    }

                    CPPH_WARN("accept failed: {}", ec.message());
                }
                else
                {
                    auto ep = sock->remote_endpoint();
                    CPPH_INFO("new client connection has been established: {}:{}",
                              ep.address().to_string(), ep.port());

                    self->notify_new_connection({++self->_unique_id_gen}, std::move(sock));
                }

                // prepare for next accept
                sock = std::make_unique<tcp::socket>(*self->io());
                self->_acceptor->async_accept(*sock, std::move(*this));
            }
        };

        _accept_fn handler{this, std::make_unique<tcp::socket>(*io())};
        _acceptor->async_accept(*handler.sock, std::move(handler));
    }

    void cleanup() override
    {
        _acceptor->close();
        _acceptor.reset();
    }

   private:
    init_info _init;
    uint64_t _unique_id_gen = 0;
    std::unique_ptr<tcp::acceptor> _acceptor;
};
}  // namespace perfkit::terminal::net::detail
