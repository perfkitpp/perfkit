#include <chrono>

#include <perfkit/perfkit.h>
#include <spdlog/spdlog.h>

#include "test_configs.hxx"

using namespace std::literals;

int main(void) {
  auto term = perfkit::create_basic_interactive_terminal();
  perfkit::terminal::initialize_with_basic_commands(term.get());

  spdlog::info("create");

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