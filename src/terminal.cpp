//
// Created by Seungwoo on 2021-09-25.
//
#include <perfkit/terminal.h>

#include "detail/basic_interactive_terminal.hpp"

namespace perfkit {

auto create_basic_interactive_terminal() -> std::shared_ptr<if_terminal> {
  return std::make_shared<basic_interactive_terminal>();
}

}  // namespace perfkit
