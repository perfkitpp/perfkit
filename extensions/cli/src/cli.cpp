//
// Created by Seungwoo on 2021-09-27.
//
#include "perfkit/extension/cli.hpp"

#include "basic_interactive_terminal.hpp"

std::shared_ptr<perfkit::if_terminal> perfkit::terminal::create_cli() {
  return std::make_shared<basic_interactive_terminal>();
}
