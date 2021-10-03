#include "perfkit/extension/net-provider.hpp"

#include "net_terminal.hpp"

perfkit::terminal_ptr perfkit::terminal::net_provider::create(const struct init_info& info) {
  return std::make_shared<net::net_terminal>(info);
}
