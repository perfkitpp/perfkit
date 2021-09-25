//
// Created by Seungwoo on 2021-09-25.
//

#include "basic_interactive_terminal.hpp"

#include <set>
#include <vector>

#include <perfkit/detail/base.hpp>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#if __linux__
#include <linenoise.h>
#endif

using namespace perfkit;
using namespace std::literals;

std::optional<std::string>
perfkit::basic_interactive_terminal::fetch_command(
        std::chrono::milliseconds timeout) {
  if (!_cmd.valid()) {
    _cmd = std::async([this]() -> std::string {
      _register_autocomplete();
      auto c_str = linenoise(_prompt.c_str());
      _unregister_autocomplete();

      std::string retval;
      if (c_str) { retval = c_str, free(c_str); }

      return retval;
    });
  }

  if (_cmd.wait_for(timeout) != std::future_status::ready) {
    return {};
  }

  return _cmd.get();
}

basic_interactive_terminal::basic_interactive_terminal() {
  _sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  spdlog::set_pattern("%+\r");
  char const *env_term = getenv("TERM");
  env_term == nullptr && (env_term = "<empty>");

  if (linenoiseIsUnsupportedTerm()) {
    spdlog::warn("linenoise unsupported for $TERM={}", env_term);
  } else {
    spdlog::info("$TERM={}", env_term);
  }
}

bool basic_interactive_terminal::set(std::string_view key, std::string_view value) {
  std::string v{key};
  std::transform(v.begin(), v.end(), v.begin(), &tolower);

  if (v == "prompt") {
    _prompt = value;
  } else {
    return false;
  }

  return true;
}

static auto locked_command_registry = std::atomic<commands::registry *>{};

void basic_interactive_terminal::_register_autocomplete() {
  if (linenoiseIsUnsupportedTerm()) { return; }

  for (commands::registry *rg = nullptr;
       !locked_command_registry.compare_exchange_strong(rg, commands());
       rg = nullptr) {
    std::this_thread::sleep_for(10ms);
  }

  auto completion = [](const char *buf, linenoiseCompletions *lc) -> int {
    int position = 0;

    auto rg = locked_command_registry.load()->root();

    std::string str = buf;
    std::vector<std::string_view> tokens;
    std::vector<commands::stroffset> offsets;
    std::vector<std::string> suggests;

    commands::tokenize_by_argv_rule(&str, tokens, &offsets);

    if (!tokens.empty()) {
      position = offsets.back().position
               + (buf[strlen(buf) - 1] == ' ') * (offsets.back().length + 1);
    }

    rg->suggest(tokens, suggests);
    for (const auto &suggest : suggests) { linenoiseAddCompletion(lc, suggest.c_str()); }

    return position;
  };

  linenoiseSetMultiLine(1);
  linenoiseSetCompletionCallback(completion);
}

void basic_interactive_terminal::_unregister_autocomplete() {
  linenoiseSetCompletionCallback(nullptr);
  locked_command_registry.store(nullptr);
}
