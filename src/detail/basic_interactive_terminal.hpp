//
// Created by Seungwoo on 2021-09-25.
//

#pragma once
#include <perfkit/detail/commands.hpp>
#include <perfkit/terminal.h>

namespace perfkit {
class basic_interactive_terminal : public if_terminal {
 public:
  commands::registry* commands() override { return &_cmd; }
  std::optional<std::string> dequeue_user_command(milliseconds timeout) override;
  void puts(std::string_view str) override;
  std::shared_ptr<spdlog::sinks::sink> sink() override;

 private:
  commands::registry _cmd;
};
}  // namespace perfkit
