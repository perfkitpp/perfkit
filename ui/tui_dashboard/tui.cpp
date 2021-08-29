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
#include "spdlog/fmt/bundled/ranges.h"
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

namespace {
}  // namespace

perfkit::detail::dashboard::dashboard(perfkit::array_view<std::string_view> args) {
  if (args.empty()) {
    spdlog::critical("invalid size of arguments provided.");
    spdlog::critical("to use, forward commandline arguments to create() function.");

    throw std::logic_error{""};
  }

  // prefrom redirection and log file generation
  _redirect_stdout(args);

  {  // setup keybaord input
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(0);

    // curs_set(1);
  }

  // register all commands
  _init_commands();

  // initialize all windows
  {
    _stdout_pane.reset(newwin(LINES - 3, 0, 1, 0));
    scrollok(_stdout_pane.get(), true);
  }
}

void perfkit::detail::dashboard::_redirect_stdout(const array_view<std::string_view>& args) {  // redirect stdout/stderr to prevent stdout print disturb TUI window
  auto prev_stdout = stdout;
  auto prev_stderr = stderr;

  auto procname = fs::path{args[0]}.filename().string();
  auto pid      = getpid();

  auto format     = fmt::format("log/{}({})-{{}}.log", procname, pid);
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

  // Now you can safely redirect logger.
  _log = spdlog::default_logger()->clone("dashboard");
  _log->set_pattern("[%I:%M:%S.%e:%n] %l: %v");
}

void perfkit::detail::dashboard::_init_commands() {
  commands()->subcommand("q", [](array_view<std::string_view> T, size_t C) { return true; });

  commands()->subcommand("w", [](array_view<std::string_view> T, size_t C) { return true; });
  commands()->subcommand("e", [](array_view<std::string_view> T, size_t C) { return true; });

  commands()->subcommand("alias", [](array_view<std::string_view> T, size_t C) { return true; });

  commands()->subcommand("monitor", [](array_view<std::string_view> T, size_t C) { return true; });

  commands()->subcommand("config", [](array_view<std::string_view> T, size_t C) { return true; });

  commands()->subcommand("dashboard-components", [](array_view<std::string_view> T, size_t C) { return true; });
  commands()->subcommand("dashboard-height", [](array_view<std::string_view> T, size_t C) { return true; });
}

namespace {
bool key_ctrl(int x) { return !!(x & 0x1f); }
}  // namespace

void perfkit::detail::dashboard::poll(bool do_content_fetch) {
  OPTS.apply_update_and_check_if_dirty();
  _context.transient = {};

  // poll for input
  // to consume focus, reset keystroke.
  std::optional<int> keystroke;
  if (auto ch = getch(); ch != -1) {
    keystroke = ch;
  } else if (auto pair = std::make_pair(LINES, COLS); pair != prev_line_col) {
    erase();
    touchwin(stdscr);
    touchwin(_stdout_pane.get());
    wrefresh(stdscr);
    prev_line_col = pair;
    _context.transient.window_resized;
  }

  // -------------------- START DRAWING ------------------------------------------------------------

  // -------------------- FETCHED CONTENT VIEW -----------------------------------------------------
  // if content fetch, do render option view & debug view

  // -------------------- RENDER STDOUT(LOG) & OUTPUT WINDOW ---------------------------------------
  // render stdout window
  _draw_stdout(_context);

  // render output window

  // -------------------- RENDER AND HANDLE PROMPT WINDOW ------------------------------------------
  // render command prompt
  mvhline(LINES - 2, 0, 0, COLS);
  _draw_prompt(_context, keystroke);

  wnoutrefresh(stdscr);
  doupdate();
  fflush(stdout);
}

void perfkit::detail::dashboard::_draw_stdout(_context_ty& context) {
  auto& tr = context.transient;
  if (tr.window_resized) {
  }

  bool content_dirty = false;
  for (char ch; (ch = fgetc(_stdout.get())) != EOF;) {
    _stdout_buf.push_rotate(ch);
    fputc(ch, _stdout_log.get());
    content_dirty = true;
  }
  for (char ch; (ch = fgetc(_stderr.get())) != EOF;) {
    _stdout_buf.push_rotate(ch);
    fputc(ch, _stderr_log.get());
    content_dirty = true;
  }

  if (content_dirty) {
    int  y, x;
    auto pane = _stdout_pane.get();
    getmaxyx(pane, y, x);

    _stdout_buf.reserve_shrink(std::max<size_t>(_stdout_buf.size(), y * x));
    int  to_print = _stdout_buf.size();
    auto it       = _stdout_buf.end() - to_print;

    wmove(pane, 0, 0);
    for (; it != _stdout_buf.end(); ++it) { waddch(pane, *it); }

    wnoutrefresh(pane);
  }
}

void perfkit::detail::dashboard::_draw_prompt(_context_ty& context, const std::optional<int>& keystroke) {  // draw input buffer
  static auto buflen = option_factory(OPTS, "+zzzz|Dashboard|Input Buffer Maxlen", 1024).min(1).make();
  if (buflen.check_dirty_and_consume()) { _input_pane.reset(newpad(1, *buflen)); }
  auto pane = _input_pane.get();

  if (*keystroke == KEY_BACKSPACE) {
    mvwdelch(pane, getcury(pane), getcurx(pane) - 1);
  } else if (keystroke) {
    // - check special characters
    //   - functional characters, escape, ctrl + key, etc ...
    auto ch = *keystroke;
    auto kc = reinterpret_cast<std::array<char, 4>&>(ch);

    static std::vector<cmdutils::stroffset> offsets;
    static std::vector<std::string>         tokens;
    static std::vector<std::string_view>    tokens_view;
    static std::vector<std::string_view>    candidates;
    std::string_view                        sharing;

    auto suggest = [&]() {
      tokens.clear(), candidates.clear(), offsets.clear();

      cmdutils::tokenize_by_argv_rule(_input, tokens, &offsets);
      tokens_view.assign(tokens.begin(), tokens.end());

      sharing = commands()->suggest(tokens_view, tokens_view.size() - 1, candidates);
      return &candidates;
    };

    if (ch == '\n') {
      werase(pane);
    } else if (ch == KEY_LEFT) {
      wmove(pane, 0, getcurx(pane) - 1);
    } else if (ch == KEY_RIGHT) {
      wmove(pane, 0, std::min<int>(_input.size(), getcurx(pane) + 1));
    } else if (ch == '\t') {
      fmt::print("# {}\n", _input);
      if (auto& candidates = *suggest(); candidates.empty() == false) {
        if (!offsets.empty()) {
          fmt::print("{}\n", offsets);

          // print sharing portion, if user has any input.
          bool        wrap_with_quotes = (sharing.find_first_of(" \t") != sharing.npos);
          std::string to_replace;
          if (wrap_with_quotes) {
            to_replace.reserve(2 + sharing.size());
            to_replace.append("\"").append(sharing).append("\"");
          } else {
            to_replace.assign(sharing.begin(), sharing.end());
          }
          mvwaddnstr(pane, 0, offsets.back().first, to_replace.c_str(), to_replace.size());
        } else {
          // else, directly input shared portion
          mvwaddnstr(pane, 0, 0, sharing.data(), sharing.size());
        }

        if (candidates.size() > 1) {
          // has more than 1 arguments ...
          // print all candidates as aligned
          _print_aligned_candidates(candidates);
        }
      }
    } else if (isprint(ch)) {
      winsch(pane, *keystroke);
      wmove(pane, 0, getcurx(pane) + 1);
    }

    _log->info("keystroke: {} 0x{:08x} '{}'",
               kc | views::transform([](auto c) { return int(c); }),
               ch, keyname(ch));

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

  mvaddstr(LINES - 1, 0, "# ");
  ::move(LINES - 1, getcurx(pane) + 2);
  pnoutrefresh(pane, 0, 0, LINES - 1, 2, LINES - 1, COLS - 3);
}

void perfkit::detail::dashboard::_print_aligned_candidates(
    std::vector<std::string_view>& candidates) const {
  std::string output;
  for (const auto& item : candidates) {
    if (output.size() == 0) {
      // first token of the line.
      output += item;
    } else {
      // set cell width as fixed value.
      bool go_newline = false;
      int  x          = output.size();
      x += 19, x -= x % 20;
      while (x < output.size() + 1) { x += 20; };

      if (x < COLS - 20) {
        while (output.size() < x) { output += ' '; }
        output += item;
      } else {
        puts(output.c_str());

        output.clear();
        output += item;
      }
    }
  }

  if (!output.empty()) {
    puts(output.c_str());
    putchar('\n');
  }
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
