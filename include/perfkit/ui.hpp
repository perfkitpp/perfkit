#pragma once
#include <chrono>
#include <functional>
#include <thread>

#include "perfkit.h"
#include "perfkit/detail/command_registry.hpp"

namespace perfkit::ui {
using namespace std::literals;
using namespace std::chrono;

/**
 * list of signals
 */
struct signal : std::exception {
  signal() = default;
};

//! thrown when program should exit.
struct sig_finish : signal {};

/**
 * list of supported backends
 */
enum class basic_backend {
  tui_dashboard,    // dashboard-style TUI interface.
  tui_interactive,  // IMGUI-style interactive
  MAX
};

class if_message_handler {
 public:
  virtual void on_subscribed_message(perfkit::messenger::message const& m) {}
  virtual void on_unsubscribed_message(perfkit::messenger::message const& m) {}
};

class if_ui {
 public:
  virtual ~if_ui() = default;

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
  virtual auto commands() -> command_registry::node* = 0;

  /**
   * Stop currently running launched thread. It has no effect if there's not any thread
   * running in background.
   */
  virtual void stop() = 0;

  /**
   * Invoke any command. This is common method to implement support for .rc file.
   */
  virtual void invoke_command(std::string_view) = 0;

  /**
   * Registers custom message handler.
   */
  virtual void custom_message_handler(if_message_handler*) {}

  /**
   * Save event hook. Triggered before serialization
   */
  virtual void on_save() {}

  /**
   * Load event hook. Triggered after full deserialization
   */
  virtual void on_load() {}

 public:
};

if_ui* create(basic_backend b, array_view<std::string_view> argv);
void   release(if_ui*);

}  // namespace perfkit::ui
