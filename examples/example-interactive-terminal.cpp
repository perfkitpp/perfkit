#include <chrono>

#include <perfkit/extension/cli.hpp>
#include <perfkit/perfkit.h>
#include <spdlog/spdlog.h>

#include "test_configs.hxx"

using namespace std::literals;

int main(void) {
  perfkit::glog()->set_level(spdlog::level::trace);

  auto term = perfkit::terminal::create_cli();
  perfkit::terminal::initialize_with_basic_commands(term.get());

  spdlog::info("create");
  spdlog::set_level(spdlog::level::trace);

  term->commands()
          ->root()
          ->add_subcommand("test space command")
          ->add_subcommand("test command 2")
          ->add_subcommand("test command 3");

  for (;;) {
    auto cmd = term->fetch_command(1000ms);
    if (!cmd.has_value() || cmd->empty()) { continue; }

    if (cmd == "q") { break; }
    if (!term->commands()->invoke_command(*cmd)) {
      spdlog::error("execution failed .. '{}'", *cmd);
    }
  }

  spdlog::info("done.");
  return 0;
}