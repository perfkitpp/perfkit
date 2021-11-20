#pragma once
#include <perfkit/common/assert.hxx>
#include <perfkit/terminal.h>

namespace perfkit::terminal::net {
class terminal {
  using init_info = terminal_init_info;

 public:
  terminal(init_info const&) {
  }
};

}  // namespace perfkit::terminal::net