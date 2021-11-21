#include <perfkit/configs.h>

#include "net-terminal.hpp"
#include "perfkit/extension/net.hpp"

perfkit::terminal_ptr perfkit::terminal::net::create(const struct terminal_init_info& info) {
  return std::make_shared<net::terminal>(info);
}

namespace perfkit::terminal::detail {
PERFKIT_T_CATEGORY(
        terminal_profile,

        // TODO: fill terminal_init_info struct with these args.
);
}  // namespace perfkit::terminal::detail

perfkit::terminal_ptr perfkit::terminal::net::create(std::string config_profile_name) {
  return perfkit::terminal_ptr();
}
