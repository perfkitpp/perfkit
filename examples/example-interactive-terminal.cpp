#include <chrono>

#include <perfkit/perfkit.h>
#include <spdlog/spdlog.h>

#include "test_configs.hxx"

using namespace std::literals;

int main(void) {
  auto term = perfkit::create_basic_interactive_terminal();
  perfkit::commands::register_config_io_commands(term->commands());
  perfkit::commands::register_config_manip_command(term->commands());
  perfkit::commands::register_logging_manip_command(term->commands());
  perfkit::commands::register_trace_manip_command(term->commands());

  spdlog::info("create");

  for (;;) {
    auto cmd = term->fetch_command(1000ms);
    if (!cmd.has_value() || cmd->empty()) { continue; }
    spdlog::info("command: {}", *cmd);

    if (cmd == "q") { break; }
    if (!term->commands()->invoke_command(*cmd)) {
      spdlog::error("execution failed .. '{}'", *cmd);
    }
  }

  spdlog::info("done.");
  return 0;
}