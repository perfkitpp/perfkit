#include "perfkit/extension/net-provider.hpp"

#include "net-terminal.hpp"

perfkit::terminal_ptr perfkit::terminal::net::create(const struct terminal_init_info& info) {
  return std::make_shared<net::terminal>(info);
}
