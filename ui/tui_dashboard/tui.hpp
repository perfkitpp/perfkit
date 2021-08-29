//
// Created by Seungwoo on 2021-08-28.
//
#pragma once
#include <fcntl.h>
#include <unistd.h>

#include "circular_queue.hxx"
#include "curses_local.h"
#include "perfkit/perfkit.h"
#include "perfkit/ui.hpp"
#include "spdlog/fwd.h"
#include "ui-tools.hpp"

namespace perfkit::detail {
/**
 * Defines set of releaser functions for created windows.
 */
struct _window_releaser {
  void operator()(WINDOW* p) noexcept { delwin(p); }
};

struct _file_releaser {
  void operator()(FILE* p) noexcept { fclose(p); }
};

struct _fd_wrapper {
  bool         operator==(nullptr_t) const noexcept { return _fd == -1; }
  bool         operator!=(nullptr_t) const noexcept { return _fd != -1; }
               operator int() const noexcept { return _fd; }
  _fd_wrapper& operator=(_fd_wrapper const&) noexcept = default;
  _fd_wrapper(_fd_wrapper const&) noexcept            = default;

  _fd_wrapper& operator=(nullptr_t) noexcept { return _fd = -1, *this; };
  int          operator*() const noexcept { return _fd; }

  _fd_wrapper() noexcept = default;
  _fd_wrapper(int fd) { _fd = fd; }

  int _fd = -1;
};

struct _fd_releaser {
  using pointer = _fd_wrapper;
  void operator()(_fd_wrapper p) noexcept { close(p); }
};

/** */
using window_ptr = std::unique_ptr<WINDOW, _window_releaser>;
using file_ptr   = std::unique_ptr<FILE, _file_releaser>;
using fd_ptr     = std::unique_ptr<_fd_wrapper, _fd_releaser>;

/**
 *
 */
class dashboard : public perfkit::ui::if_ui {
 public:
  dashboard(array_view<std::string_view>);

 public:
  void poll(bool do_content_fetch) override;
  void launch(std::chrono::milliseconds interval, int content_fetch_cycle) override;
  auto commands() -> ui::command_register::node* override;
  void stop() override;
  void invoke_command(std::string_view view) override;

 private:
  void _argparse(array_view<std::string_view>);
  void _output(std::string_view str);

 private:
 private:
  std::shared_ptr<spdlog::logger> _log;

  window_ptr           _option_pane;
  window_ptr           _message_pane;
  window_ptr           _stdout_pane;
  window_ptr           _output_pane;
  window_ptr           _input_pane;
  ui::command_register _commands;

  file_ptr             _stdout;
  file_ptr             _stderr;
  file_ptr             _stdout_log;
  file_ptr             _stderr_log;
  fd_ptr               _pipe_stdout[2];
  fd_ptr               _pipe_stderr[2];
  circular_queue<char> _stdout_buf{65535};

  std::string         _input;
  std::pair<int, int> prev_line_col = {};

  struct _context_ty {
    struct _transient {
      bool window_resized;
      int  cur_h;
    } transient;

  } _context;

  void        _draw_prompt(_context_ty& context, const std::optional<int>& keystroke);
  void        _draw_stdout(_context_ty& context);
  void        _init_commands();
  void        _redirect_stdout(const array_view<std::string_view>& args);
  void        _print_aligned_candidates(std::vector<std::string_view>& candidates) const;
};
}  // namespace perfkit::detail