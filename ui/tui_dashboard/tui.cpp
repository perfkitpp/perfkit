//
// Created by Seungwoo on 2021-08-28.
//
#include "tui.hpp"

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <filesystem>

#include "range/v3/action/insert.hpp"
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

#define NS_DASHBOARD "+zzzz|Dashboard|"
PERFKIT_OPTION_DISPATCHER(OPTS);

namespace {
auto opt_message_pane_size = option_factory(OPTS, NS_DASHBOARD "View|Messages", 10).make();
auto opt_options_pane_size = option_factory(OPTS, NS_DASHBOARD "View|Options", 10).make();
auto opt_stdout_pane_size  = option_factory(OPTS, NS_DASHBOARD "View|Stdout", 1000).make();
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
  commands()->subcommand("q", [&](array_view<std::string_view> argv) -> bool { throw ui::sig_finish{}; });

  commands()->subcommand("w", [&](array_view<std::string_view> argv) -> bool { return fmt::print("{}\n", argv), true; });
  commands()->subcommand("e", [&](array_view<std::string_view> argv) -> bool { return fmt::print("{}\n", argv), true; });

  commands()->subcommand(
      "set",
      [&](array_view<std::string_view> argv) -> bool {  //
        return fmt::print("{}\n", argv), true;
      },
      [&](auto hint, std::set<std::string>& r) {
        insert(r, option_dispatcher::_all()
                      | views::values
                      | views::transform([&](auto ptr) {
                          return std::string(ptr->display_key());
                        }));
      });

  commands()->subcommand("messenger", [&](array_view<std::string_view> argv) -> bool { return fmt::print("{}\n", argv), true; });
  commands()->subcommand("get", [&](array_view<std::string_view> argv) -> bool { return fmt::print("{}\n", argv), true; });
}

namespace {
bool key_ctrl(int x) { return !!(x & 0x1f); }
}  // namespace

void perfkit::detail::dashboard::poll(bool do_content_fetch) {
  OPTS.apply_update_and_check_if_dirty();
  _context.transient                 = {};
  _context.transient.update_contents = do_content_fetch;

  // poll for input
  // to consume focus, reset keystroke.
  std::optional<int> keystroke;
  if (auto ch = getch(); ch != -1) {
    keystroke = ch;
  } else if (auto pair = std::make_pair(LINES, COLS); pair != prev_line_col) {
    erase();
    touchwin(stdscr);
    wrefresh(stdscr);
    prev_line_col                     = pair;
    _context.transient.window_resized = true;
  }

  // -------------------- START DRAWING ------------------------------------------------------------

  // -------------------- FETCHED CONTENT VIEW -----------------------------------------------------
  // if content fetch, do render option view & debug view
  _draw_messages(_context, keystroke);
  _draw_options(_context, keystroke);

  // -------------------- RENDER STDOUT(LOG) & OUTPUT WINDOW ---------------------------------------
  // render stdout window
  _draw_stdout(_context);

  // render output window

  // -------------------- RENDER AND HANDLE PROMPT WINDOW ------------------------------------------
  // render command prompt
  _draw_prompt(_context, keystroke);

  doupdate();
  fflush(stdout);
}

void dashboard::_draw_messages(_context_ty& context, const std::optional<int>& keystroke) {
  bool  content_dirty = _layout(context, _message_pane, *opt_message_pane_size, " MESSAGES ");
  auto& tr            = context.transient;
}

void dashboard::_draw_options(_context_ty& context, const std::optional<int>& keystroke) {
}

void perfkit::detail::dashboard::_draw_stdout(_context_ty& context) {
  bool  content_dirty = _layout(context, _stdout_pane, *opt_stdout_pane_size, " OUTPUT ");
  auto& tr            = context.transient;
  auto  pane          = _stdout_pane.get();

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
    int y, x;
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
  static auto buflen = option_factory(OPTS, NS_DASHBOARD "Input Buffer Maxlen", 1024).min(1).make();
  if (buflen.check_dirty_and_consume()) { _input_pane.reset(newpad(1, *buflen)); }
  auto pane = _input_pane.get();

  if (keystroke) {
    // - check special characters
    //   - functional characters, escape, ctrl + key, etc ...
    auto ch = *keystroke;
    auto kc = reinterpret_cast<std::array<char, 4>&>(ch);

    static std::vector<cmdutils::stroffset> offsets;
    static std::vector<std::string>         tokens;
    static std::vector<std::string_view>    tokens_view;
    static std::vector<std::string>         candidates;
    std::string                             sharing;

    auto suggest = [&]() {
      tokens.clear(), candidates.clear(), offsets.clear();

      cmdutils::tokenize_by_argv_rule(_input, tokens, &offsets);
      tokens_view.assign(tokens.begin(), tokens.end());

      sharing = commands()->suggest(tokens_view, candidates);
      return &candidates;
    };

    if (ch == '\n') {
      if (_input.empty() && !_history.empty()) {
        _input = _history.back();
      }

      if (!_input.empty()) {
        // tokenize
        tokens.clear(), offsets.clear();
        cmdutils::tokenize_by_argv_rule(_input, tokens, &offsets);
        tokens_view.assign(tokens.begin(), tokens.end());

        // suggest
        commands()->invoke(tokens_view);
        _history_cursor = 0;

        if (_history.empty() || _history.back() != _input) {
          _history.push_rotate(std::move(_input));
        }
      }
      werase(pane);
    } else if (ch == KEY_HOME) {
      wmove(pane, 0, 0);
    } else if (ch == KEY_END) {
      wmove(pane, 0, _input.size());
    } else if (ch == KEY_BACKSPACE) {
      mvwdelch(pane, 0, getcurx(pane) - 1);
    } else if (ch == KEY_UP || ch == KEY_DOWN) {
      _history_cursor += (ch == KEY_UP) - (ch == KEY_DOWN);
      _history_cursor = std::clamp<int>(_history_cursor, 0, _history.size());

      if (auto it = _history.end() - _history_cursor; it != _history.end()) {
        werase(pane);
        mvwaddstr(pane, 0, 0, it->c_str());
      }
    } else if (ch == KEY_LEFT) {
      wmove(pane, 0, getcurx(pane) - 1);
    } else if (ch == KEY_RIGHT) {
      wmove(pane, 0, std::min<int>(_input.size(), getcurx(pane) + 1));
    } else if (ch == '\t') {
      if (auto& candidates = *suggest(); candidates.empty() == false) {
        if (!offsets.empty()) {
          // print sharing portion, if user has any input.
          bool        escape_spacechar = (sharing.find_first_of(' ') != sharing.npos);
          std::string to_replace;
          if (escape_spacechar) {
            to_replace = std::regex_replace(sharing, std::regex{" "}, "\\ ");
          } else {
            to_replace.assign(sharing.begin(), sharing.end());
          }
          mvwaddnstr(pane, 0, offsets.back().first, to_replace.c_str(), to_replace.size());
          wmove(pane, 0, offsets.back().first + std::max(to_replace.size(), offsets.back().second));
        } else if (sharing.size()) {
          // else, directly input shared portion
          mvwaddnstr(pane, 0, 0, sharing.data(), sharing.size());
          wmove(pane, 0, sharing.size());
        }

        if (candidates.size() > 1) {
          // has more than 1 arguments ...
          // print all candidates as aligned
          fmt::print("  # {:-<{}}", _input += ' ', COLS - 6);
          _print_aligned_candidates(candidates);
        }
      }
    } else if (isprint(ch)) {
      winsch(pane, *keystroke);
      wmove(pane, 0, getcurx(pane) + 1);
    }

    _log->debug("keystroke: {} 0x{:08x} '{}'",
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

  mvaddstr(LINES - 2, 0, "# ");
  ::move(LINES - 2, getcurx(pane) + 2);

  pnoutrefresh(pane, 0, 0, LINES - 2, 2, LINES - 2, COLS - 3);

  // TODO:
  // draw status bar.
}

void perfkit::detail::dashboard::_print_aligned_candidates(
    const std::vector<std::string>& candidates) const {
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

bool dashboard::_layout(
    dashboard::_context_ty& context, window_ptr& wnd, int pane_size, const char* name) {
  auto& tr = context.transient;

  bool content_dirty = false;
  auto offset        = tr.cur_h;
  auto pane          = _stdout_pane.get();

  static std::map<std::string, std::tuple<int, int>> pane_size_changes;

  auto& [prev_offset, prev_size] = pane_size_changes[name];
  pane_size                      = std::min(LINES - 2 - offset, pane_size);
  bool pane_size_changed         = prev_size != pane_size;
  prev_size                      = pane_size;
  prev_offset                    = offset;
  tr.cur_h += pane_size;

  if (tr.window_resized || pane_size_changed) {
    wnd.reset(pane = newwin(pane_size - 2, COLS - 2, offset + 1, 1));
    scrollok(pane, true);

    content_dirty = true;

    window_ptr tmp{newwin(pane_size, COLS, offset, 0)};
    ::box(tmp.get(), 0, 0), mvwaddstr(tmp.get(), 0, 4, name);
    wnoutrefresh(tmp.get());
  }

  return content_dirty;
}