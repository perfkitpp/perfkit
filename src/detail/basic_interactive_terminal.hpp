//
// Created by Seungwoo on 2021-09-25.
//

#pragma once
#include <future>

#include <perfkit/detail/commands.hpp>
#include <perfkit/terminal.h>

namespace perfkit {
class basic_interactive_terminal : public if_terminal {
 public:
  commands::registry* commands() override { return &_registry; }
  std::optional<std::string> fetch_command(milliseconds timeout) override;
  void puts(std::string_view str) override { ::fwrite(str.data(), str.size(), 1, stdout); }
  std::shared_ptr<spdlog::sinks::sink> sink() override { return _sink; }

  basic_interactive_terminal();

  bool set(std::string_view key, std::string_view value) override;

 private:
  void _register_autocomplete();
  void _unregister_autocomplete();

 private:
  commands::registry _registry;
  std::shared_ptr<spdlog::sinks::sink> _sink;
  std::future<std::string> _cmd;

  std::string _prompt = "$ ";
};
}  // namespace perfkit