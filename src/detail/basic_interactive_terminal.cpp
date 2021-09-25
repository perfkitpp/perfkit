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

std::optional<std::string>
perfkit::basic_interactive_terminal::fetch_command(
        std::chrono::milliseconds timeout) {
  if (!_cmd.valid()) {
    _cmd = std::async([this]() -> std::string {
      _register_autocomplete();
      auto c_str = linenoise(_prompt.c_str());

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

void basic_interactive_terminal::_register_autocomplete() {
  auto completion = [](const char *buf, linenoiseCompletions *lc) ->int{
    glog()->info("completion call for {}", buf);
    linenoiseAddCompletion(lc, "hello");
    linenoiseAddCompletion(lc, "hello there");

    if (buf[0] == 'h') {
      linenoiseAddCompletion(lc, "hello");
      linenoiseAddCompletion(lc, "hello there");
    }

    return 0;
  };

  linenoiseSetMultiLine(1);
  linenoiseSetCompletionCallback(completion);
}
