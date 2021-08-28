//
// Created by Seungwoo on 2021-08-28.
//
#include "tui.hpp"

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <filesystem>

#include "range/v3/algorithm.hpp"
#include "range/v3/range.hpp"
#include "range/v3/view.hpp"
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;
using namespace perfkit;
using namespace perfkit::detail;
using namespace ranges;

#define VERIFY(EXPR) \
  if (!(EXPR)) {     \
    assert(false);   \
    throw;           \
  }

PERFKIT_OPTION_DISPATCHER(OPTS);

perfkit::detail::dashboard::dashboard(perfkit::array_view<std::string_view> args) {
  if (args.empty()) {
    spdlog::critical("invalid size of arguments provided.");
    spdlog::critical("to use, forward commandline arguments to create() function.");
    throw std::logic_error{""};
  }

  {  // redirect stdout/stderr to prevent stdout print disturb TUI window
    auto prev_stdout = stdout;
    auto prev_stderr = stderr;

    auto procname = fs::path{args[0]}.filename().string();
    auto pid      = getpid();

    auto format     = fmt::format("log/{}[{}]-{{}}.log", procname, pid);
    auto log_stdout = fmt::format(format, "stdout");
    auto log_stderr = fmt::format(format, "stderr");

    _stdout_log.reset(fopen(log_stdout.c_str(), "a"));
    _stderr_log.reset(fopen(log_stderr.c_str(), "a"));

    int fd[2];
    VERIFY(pipe2(fd, O_NONBLOCK) == 0);
    _pipe_stdout[0].reset(fd[0]);
    _pipe_stdout[1].reset(fd[1]);

    VERIFY(pipe2(fd, O_NONBLOCK) == 0);
    _pipe_stderr[0].reset(fd[0]);
    _pipe_stderr[1].reset(fd[1]);

    fs::create_directories("log");
    stdout = fdopen(_pipe_stdout[1].get(), "w");
    stderr = fdopen(_pipe_stderr[1].get(), "w");

    _stdout.reset(fdopen(_pipe_stdout[0].get(), "r"));
    _stderr.reset(fdopen(_pipe_stderr[0].get(), "r"));

    set_term(newterm(nullptr, prev_stdout, prev_stderr));
  }
  {
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(0);

    // curs_set(1);
  }

  // register all commands
  {}

  // initialize all windows
  {
    _stdout_pane.reset(newwin(LINES - 3, 0, 1, 0));
    scrollok(_stdout_pane.get(), true);
  }
}

void perfkit::detail::dashboard::poll(bool do_content_fetch) {
  OPTS.apply_update_and_check_if_dirty();

  // poll for input
  // to consume focus, reset keystroke.
  std::optional<int> keystroke;
  if (auto ch = getch(); ch != -1) {
    keystroke = ch;
  } else if (auto pair = std::make_pair(LINES, COLS); pair != prev_line_col) {
    erase();
    touchwin(stdscr);
    wrefresh(stdscr);
    prev_line_col = pair;
  }

  // -------------------- START DRAWING ------------------------------------------------------------
  mvhline(0, 0, 0, COLS);

  // -------------------- FETCHED CONTENT VIEW -----------------------------------------------------
  // if content fetch, do render option view & debug view

  // -------------------- RENDER STDOUT(LOG) & OUTPUT WINDOW ---------------------------------------
  // render stdout window
  for (char ch; (ch = fgetc(_stdout.get())) != EOF;) {
    waddch(_stdout_pane.get(), ch);
    fputc(ch, _stdout_log.get());
  }
  for (char ch; (ch = fgetc(_stderr.get())) != EOF;) {
    waddch(_stdout_pane.get(), ch);
    fputc(ch, _stderr_log.get());
  }
  wnoutrefresh(_stdout_pane.get());

  // render output window

  // -------------------- RENDER AND HANDLE PROMPT WINDOW ------------------------------------------
  // render command prompt
  mvhline(LINES - 2, 0, 0, COLS);
  {  // draw input buffer
    static auto buflen = option_factory(OPTS, "+zzzz|Dashboard|", 1024).min(1).make();
    if (buflen.check_dirty_and_consume()) { _input_pane.reset(newpad(1, *buflen)); }
    auto pane = _input_pane.get();

    if (*keystroke == KEY_BACKSPACE) {
      mvwdelch(pane, getcury(pane), getcurx(pane) - 1);
    } else if (keystroke) {
      // - check special characters
      //   - functional characters, escape, ctrl + key, etc ...
      auto ch = *keystroke;

      if (ch == '\n') {
        werase(pane);
      } else if (ch == KEY_LEFT) {
        wmove(pane, 0, getcurx(pane) - 1);
      } else if (ch == KEY_RIGHT) {
        wmove(pane, 0, std::min<int>(_input.size(), getcurx(pane) + 1));
      } else if (isprint(ch)) {
        winsch(pane, *keystroke);
        wmove(pane, 0, getcurx(pane) + 1);
      }

      spdlog::info("keystroke: {}", ch);

      {  // Refresh input buffer every keystroke
        int x, y;
        _input.resize(*buflen);
        getyx(pane, y, x);
        mvwinnstr(pane, 0, 0, _input.data(), _input.size());

        auto idx = _input.find_last_not_of(' ');
        _input.resize(idx != _input.npos ? idx + 1 : 0);
        wmove(pane, y, x);
      }
    }

    mvaddch(LINES - 1, 0, '#');
    ::move(LINES - 1, getcurx(pane) + 2);
    pnoutrefresh(pane, 0, 0, LINES - 1, 2, LINES - 1, COLS - 3);
  }

  wnoutrefresh(stdscr);
  doupdate();
  fflush(stdout);
}

auto perfkit::detail::dashboard::commands() -> perfkit::ui::command_register::node* {
  return _commands._get_root();
}

void perfkit::detail::dashboard::launch(std::chrono::milliseconds interval, int content_fetch_cycle) {
}

void perfkit::detail::dashboard::invoke_command(std::string_view view) {
}

void perfkit::detail::dashboard::stop() {
}

void dashboard::_output(std::string_view str) {
}
