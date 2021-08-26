#pragma once
#include <chrono>
#include <functional>
#include <thread>

#include "detail/array_view.hxx"

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
  virtual void poll(bool do_content_fetch) = 0;
  virtual void launch(milliseconds interval, int content_fetch_cycle) = 0;

 public:
};

if_backend* create(basic_backend b);
void release(if_backend*);

}  // namespace perfkit::ui
