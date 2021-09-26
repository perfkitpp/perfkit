#include <chrono>

#include <perfkit/perfkit.h>
#include <spdlog/spdlog.h>

#include "test_configs.hxx"

using namespace std::literals;

int main(void) {
  perfkit::glog()->set_level(spdlog::level::trace);

  auto term = perfkit::create_basic_interactive_terminal();
  perfkit::terminal::initialize_with_basic_commands(term.get());

  spdlog::info("create");
  spdlog::set_level(spdlog::level::trace);

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