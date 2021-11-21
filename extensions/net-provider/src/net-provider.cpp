#include "perfkit/extension/net-provider.hpp"

#include <perfkit/configs.h>

#include "net-terminal.hpp"

perfkit::terminal_ptr perfkit::terminal::net::create(const struct terminal_init_info& info) {
  return std::make_shared<net::terminal>(info);
}

namespace perfkit::terminal::detail {
PERFKIT_T_CATEGORY(
        terminal_profile,


        );
}  // namespace perfkit::terminal::detail

perfkit::terminal_ptr perfkit::terminal::net::create(std::string config_profile_name) {
  return perfkit::terminal_ptr();
}
