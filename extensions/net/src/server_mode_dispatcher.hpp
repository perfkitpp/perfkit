
//
// Created by ki608 on 2021-11-21.
//

#pragma once
#include "basic_dispatcher_impl.hpp"

namespace perfkit::terminal::net::detail {
struct server_mode_dispatcher_init_info {
  std::string bind_addr = "0.0.0.0";
  uint16_t bind_port    = 0;
};

class server_mode_dispatcher : public basic_dispatcher_impl {
 public:
  using init_info = server_mode_dispatcher_init_info;
  server_mode_dispatcher(
          basic_dispatcher_impl::init_info const& base, init_info const& init)
          : basic_dispatcher_impl(base),
            _init(init) {}

 protected:
  void refresh() override {
    if (_acceptor && _acceptor->is_open())
      _acceptor->close();  // just try close.

    _acceptor.reset(new tcp::acceptor{*io()});
    tcp::endpoint endpoint{asio::ip::address::from_string(_init.bind_addr), _init.bind_port};

    _acceptor->bind(endpoint);
    _acceptor->listen();

    struct _accept_fn {
      server_mode_dispatcher* self;
      std::unique_ptr<tcp::socket> sock;

      void operator()(asio::error_code const& ec) {
        auto CPPH_LOGGER = [this] { return self->CPPH_LOGGER(); };

        if (ec) {
          if (ec == asio::error::operation_aborted) {
            CPPH_INFO("aborting accept opeartion");
            return;
          }

          CPPH_WARN("accept failed: {}", ec.message());
        } else {
          self->notify_new_connection({++self->_unique_id_gen}, std::move(sock));

          auto ep = sock->remote_endpoint();
          CPPH_INFO("new client connection has been established: {}:{}",
                    ep.address().to_string(), ep.port());
        }

        // prepare for next accept
        sock = std::make_unique<tcp::socket>(*self->io());
        self->_acceptor->async_accept(*sock, std::move(*this));
      }
    };

    _accept_fn handler{this, std::make_unique<tcp::socket>(*io())};
    _acceptor->async_accept(*handler.sock, std::move(handler));
  }

  void cleanup() override {
    _acceptor->close();
    _acceptor.reset();
  }

 private:
  init_info _init;
  uint64_t _unique_id_gen = 0;
  std::unique_ptr<tcp::acceptor> _acceptor;
};
}  // namespace perfkit::terminal::net::detail
