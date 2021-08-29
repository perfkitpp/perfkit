#include "perfkit/ui.hpp"

#include <cassert>
#include <chrono>

#include "tui_dashboard/tui.hpp"
#include "ui-tools.hpp"

namespace perfkit::ui {
extern if_ui* g_ui;

if_ui* create(basic_backend b, array_view<std::string_view> argv) {
  if (g_ui) { throw "ONLY SINGLE UI INSTANCE AT THE SAME TIME IS ALLOWED!!!"; }

  switch (b) {
    case basic_backend::tui_dashboard:
      return (g_ui = new perfkit::detail::dashboard{argv});
    case basic_backend::tui_interactive:
      return nullptr;
    default:
      throw "Invalid BACKEND switch!!!";
  }
}

void release() {
  delete g_ui;
  g_ui = nullptr;
}

if_ui* get() {
  return g_ui;
}

}  // namespace perfkit::ui
