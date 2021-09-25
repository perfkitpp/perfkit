//
// Created by Seungwoo on 2021-09-25.
//

#include "basic_interactive_terminal.hpp"

#include <set>
#include <vector>

using namespace perfkit;

std::optional<std::string> perfkit::basic_interactive_terminal::dequeue_user_command(std::chrono::milliseconds timeout) {
  return std::optional<std::string>();
}

void perfkit::basic_interactive_terminal::puts(std::string_view str) {
}

std::shared_ptr<spdlog::sinks::sink> perfkit::basic_interactive_terminal::sink() {
  return std::shared_ptr<spdlog::sinks::sink>();
}
