//
// Created by Seungwoo on 2021-09-30.
//

#pragma once
#include "perfkit/terminal.h"

class net_terminal : public perfkit::if_terminal {
 public:
  net_terminal();

 public:
  perfkit::commands::registry* commands() override;
  std::optional<std::string> fetch_command(std::chrono::milliseconds timeout) override;
  void push_command(std::string_view command) override;
  void write(std::string_view str, perfkit::color fg, perfkit::color bg) override;
  std::shared_ptr<spdlog::sinks::sink> sink() override;
};
