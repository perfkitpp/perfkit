//
// Created by Seungwoo on 2021-10-02.
//

#include "net_session.hpp"

perfkit::commands::registry* perfkit::net::net_session::commands() {
  return nullptr;
}

std::optional<std::string> perfkit::net::net_session::fetch_command(std::chrono::milliseconds timeout) {
  return {};
}

void perfkit::net::net_session::push_command(std::string_view command) {
}

void perfkit::net::net_session::write(std::string_view str, perfkit::color fg, perfkit::color bg) {
}
