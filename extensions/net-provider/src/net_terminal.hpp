//
// Created by Seungwoo on 2021-09-30.
//

#pragma once
#include <thread>

#include "perfkit/extension/net-provider.hpp"
#include "perfkit/terminal.h"

namespace perfkit::net {
class net_session;

class net_terminal : public perfkit::if_terminal {
 public:
  net_terminal(terminal::net_provider::init_info const&);
  ~net_terminal();

 public:
  perfkit::commands::registry* commands() override;
  std::optional<std::string> fetch_command(std::chrono::milliseconds timeout) override;
  void push_command(std::string_view command) override;
  void write(std::string_view str, perfkit::color fg, perfkit::color bg) override;
  std::shared_ptr<spdlog::sinks::sink> sink() override;

 private:
  std::unique_ptr<net_session> _session;
  terminal::net_provider::init_info _init_cached;
};
}  // namespace perfkit::net
