//
// Created by Seungwoo on 2021-09-30.
//

#include "net_terminal.hpp"

#include "net_session.hpp"
#include "perfkit/_net/net-proto.hpp"

namespace perfkit::net {

perfkit::commands::registry* net_terminal::commands() {
  return nullptr;
}

std::optional<std::string> net_terminal::fetch_command(std::chrono::milliseconds timeout) {
  // Retrieve single command from net-command-queue.

  return std::optional<std::string>();
}

void net_terminal::push_command(std::string_view command) {
}

void net_terminal::write(std::string_view str, perfkit::color fg, perfkit::color bg) {
}

std::shared_ptr<spdlog::sinks::sink> net_terminal::sink() {
  return std::shared_ptr<spdlog::sinks::sink>();
}

net_terminal::net_terminal(const terminal::net_provider::init_info& info)
        : _init_cached{info} {
}

net_terminal::~net_terminal()
        = default;

}  // namespace perfkit::net
