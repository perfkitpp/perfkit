#pragma once
#include <chrono>
#include <functional>
#include <thread>

#include "perfkit/detail/array_view.hxx"
#include "perfkit/detail/command_register.hpp"

namespace perfkit::ui {
using namespace std::literals;
using namespace std::chrono;

enum class basic_backend {
  tui_dashboard,    // dashboard-style TUI interface.
  tui_interactive,  // IMGUI-style interactive
};

class if_backend {
 public:
  virtual ~if_backend() = 0;

 public:
  /**
   * Poll once
   * @param do_content_fetch Determines whether to fetch and consume messages.
   */
  virtual void poll(bool do_content_fetch) = 0;

  /**
   * Launch separate thread dedicate for polling
   * @param interval Polling interval.
   * @param content_fetch_cycle Internal loop will poll once per n times.
   */
  virtual void launch(milliseconds interval, int content_fetch_cycle = 1) = 0;

  /**
   * Gets internal command register
   * @return
   */
  virtual auto commands() -> command_register::node* = 0;

  /**
   * Stop currently running launched thread. It has no effect if there's not any thread
   * running in background.
   */
  virtual void stop() = 0;

  /**
   * Invoke any command. This is common method to implement support for .rc file.
   */
  virtual void invoke_command(std::string_view) = 0;

 public:
};

if_backend* create(basic_backend b);
void release(if_backend*);

}  // namespace perfkit::ui
