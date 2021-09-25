//
// Created by Seungwoo on 2021-09-25.
//

#include "basic_interactive_terminal.hpp"

#include <set>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>

#if __linux__
#include <sys/poll.h>
#include <sys/unistd.h>
#endif

using namespace perfkit;

std::optional<std::string> perfkit::basic_interactive_terminal::fetch_command(std::chrono::milliseconds timeout) {
  pollfd pollee = {};
  pollee.fd     = STDIN_FILENO;
  pollee.events = POLLIN;

  if (poll(&pollee, 1, timeout.count()) <= 0) { return {}; }

  std::string retval;
  retval.reserve(1024);

  for (int ch; (ch = fgetc(stdin)) != '\n' && ch != EOF;) { retval.push_back(ch); }
  if (retval.empty()) { return {}; }

  return retval;
}

basic_interactive_terminal::basic_interactive_terminal() {
  _sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
}
