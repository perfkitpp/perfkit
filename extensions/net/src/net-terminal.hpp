#pragma once
#include <perfkit/common/assert.hxx>
#include <perfkit/extension/net.hpp>
#include <perfkit/terminal.h>

#include "dispatcher.hpp"
#include "utils.hpp"

namespace perfkit::terminal::net {
class terminal : public if_terminal {
  using init_info = terminal_init_info;

 public:
  explicit terminal(init_info const& init) : _io(init) {
    // register number of
  }

  commands::registry* commands() override {
    return nullptr;
  }
  std::optional<std::string> fetch_command(milliseconds timeout) override {
    return std::optional<std::string>();
  }
  void push_command(std::string_view command) override {
  }
  void write(std::string_view str, termcolor fg, termcolor bg) override {
  }
  std::shared_ptr<spdlog::sinks::sink> sink() override {
    return std::shared_ptr<spdlog::sinks::sink>();
  }

 private:
  dispatcher _io;
};

}  // namespace perfkit::terminal::net