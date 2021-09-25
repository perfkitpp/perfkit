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
  std::optional<std::string> fetch_command(milliseconds timeout) override;
  void puts(std::string_view str) override { ::fwrite(str.data(), str.size(), 1, stdout); }
  std::shared_ptr<spdlog::sinks::sink> sink() override { return _sink; }

  basic_interactive_terminal();

 private:
  commands::registry _cmd;
  std::shared_ptr<spdlog::sinks::sink> _sink;
};
}  // namespace perfkit
