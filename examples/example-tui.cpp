#include "ftxui/component/captured_mouse.hpp"      // for ftxui
#include "ftxui/component/component.hpp"           // for Checkbox, Vertical
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "perfkit/configs.h"
#include "perfkit/ftxui-extension.hpp"

using namespace std::literals;

std::map<std::string, std::map<std::string, std::string>> ved{
    {"asd", {{"asd", "weqw"}, {"vafe, ewqew", "dwrew"}}},
    {"vadsfew", {{"dav ,ea w", "Ewqsad"}, {"scxz ss", "dwqewqew"}}}};

PERFKIT_CATEGORY(cfg) {
  PERFKIT_CONFIGURE(active, true).confirm();
  PERFKIT_SUBCATEGORY(labels) {
    PERFKIT_CONFIGURE(foo, 1).confirm();
    PERFKIT_CONFIGURE(bar, false).confirm();
    PERFKIT_CONFIGURE(ce, "ola ollalala").confirm();
    PERFKIT_CONFIGURE(ced, std::vector({1, 2, 3, 4, 5, 6})).confirm();
    PERFKIT_CONFIGURE(cedr, (std::map<std::string, int>{{"fdf", 2}, {"erwe", 4}})).confirm();
    PERFKIT_CONFIGURE(bb, (std::map<std::string, bool>{{"fdf", false}, {"erwe", true}})).confirm();
    PERFKIT_CONFIGURE(cedrs, 3.141592).confirm();
    PERFKIT_CONFIGURE(cedrstt, std::move(ved)).confirm();
  }
  PERFKIT_SUBCATEGORY(lomo) {
    PERFKIT_SUBCATEGORY(movdo) {
      PERFKIT_CONFIGURE(ce, 1).confirm();
      PERFKIT_CONFIGURE(ced, 1).confirm();
      PERFKIT_CONFIGURE(cedr, 1).confirm();

      PERFKIT_SUBCATEGORY(cef) {
      }
      PERFKIT_SUBCATEGORY(ccra) {
        PERFKIT_CONFIGURE(foo, 1).confirm();
        PERFKIT_CONFIGURE(bar, 1).confirm();
        PERFKIT_CONFIGURE(ce, 1).confirm();
        PERFKIT_CONFIGURE(ced, 1).confirm();
        PERFKIT_CONFIGURE(cedr, 1).confirm();
        PERFKIT_CONFIGURE(cedrs, 1).confirm();

        PERFKIT_CONFIGURE(a_foo, 1).confirm();
        PERFKIT_CONFIGURE(a_bar, 1).confirm();
        PERFKIT_CONFIGURE(a_ce, 1).confirm();
        PERFKIT_CONFIGURE(a_ced, 1).confirm();
        PERFKIT_CONFIGURE(a_cedr, 1).confirm();
        PERFKIT_CONFIGURE(a_cedrs, 1).confirm();

        PERFKIT_CONFIGURE(b_foo, 1).confirm();
        PERFKIT_CONFIGURE(b_bar, 1).confirm();
        PERFKIT_CONFIGURE(b_ce, 1).confirm();
        PERFKIT_CONFIGURE(b_ced, 1).confirm();
        PERFKIT_CONFIGURE(b_cedr, 1).confirm();
        PERFKIT_CONFIGURE(b_cedrs, 1).confirm();

        PERFKIT_CONFIGURE(c_foo, 1).confirm();
        PERFKIT_CONFIGURE(c_bar, 1).confirm();
        PERFKIT_CONFIGURE(c_ce, 1).confirm();
        PERFKIT_CONFIGURE(c_ced, 1).confirm();
        PERFKIT_CONFIGURE(c_cedr, 1).confirm();
        PERFKIT_CONFIGURE(c_cedrs, 1).confirm();

        PERFKIT_CONFIGURE(d_foo, 1).confirm();
        PERFKIT_CONFIGURE(d_bar, 1).confirm();
        PERFKIT_CONFIGURE(d_ce, 1).confirm();
        PERFKIT_CONFIGURE(d_ced, 1).confirm();
        PERFKIT_CONFIGURE(d_cedr, 1).confirm();
        PERFKIT_CONFIGURE(d_cedrs, 1).confirm();
      }
    }
  }

  PERFKIT_CONFIGURE(foo, 1).confirm();
  PERFKIT_CONFIGURE(bar, 1).confirm();
  PERFKIT_CONFIGURE(ce, 1).confirm();
  PERFKIT_CONFIGURE(ced, 1).confirm();
  PERFKIT_CONFIGURE(cedr, 1).confirm();
  PERFKIT_CONFIGURE(cedrs, 1).confirm();

  PERFKIT_CONFIGURE(a_foo, 1).confirm();
  PERFKIT_CONFIGURE(a_bar, 1).confirm();
  PERFKIT_CONFIGURE(a_ce, 1).confirm();
  PERFKIT_CONFIGURE(a_ced, 1).confirm();
  PERFKIT_CONFIGURE(a_cedr, 1).confirm();
  PERFKIT_CONFIGURE(a_cedrs, 1).confirm();

  PERFKIT_CONFIGURE(b_foo, 1).confirm();
  PERFKIT_CONFIGURE(b_bar, 1).confirm();
  PERFKIT_CONFIGURE(b_ce, 1).confirm();
  PERFKIT_CONFIGURE(b_ced, 1).confirm();
  PERFKIT_CONFIGURE(b_cedr, 1).confirm();
  PERFKIT_CONFIGURE(b_cedrs, 1).confirm();

  PERFKIT_CONFIGURE(c_foo, 1).confirm();
  PERFKIT_CONFIGURE(c_bar, 1).confirm();
  PERFKIT_CONFIGURE(c_ce, 1).confirm();
  PERFKIT_CONFIGURE(c_ced, 1).confirm();
  PERFKIT_CONFIGURE(c_cedr, 1).confirm();
  PERFKIT_CONFIGURE(c_cedrs, 1).confirm();

  PERFKIT_CONFIGURE(d_foo, 1).confirm();
  PERFKIT_CONFIGURE(d_bar, 1).confirm();
  PERFKIT_CONFIGURE(d_ce, 1).confirm();
  PERFKIT_CONFIGURE(d_ced, 1).confirm();
  PERFKIT_CONFIGURE(d_cedr, 1).confirm();
  PERFKIT_CONFIGURE(d_cedrs, 1).confirm();

  PERFKIT_CONFIGURE(e_foo, 1).confirm();
  PERFKIT_CONFIGURE(e_bar, 1).confirm();
  PERFKIT_CONFIGURE(e_ce, 1).confirm();
  PERFKIT_CONFIGURE(e_ced, 1).confirm();
  PERFKIT_CONFIGURE(e_cedr, 1).confirm();
  PERFKIT_CONFIGURE(e_cedrs, 1).confirm();
}

PERFKIT_CATEGORY(vlao) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao1) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao2) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao3) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao4) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao5) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao6) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao7) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao8) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao9) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao22) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao33) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao44) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }
PERFKIT_CATEGORY(vlao55) { PERFKIT_CONFIGURE(e_cedrs, 1).confirm(); }

using namespace ftxui;

int main(int argc, const char* argv[]) {
  auto component = perfkit_fxtui::config_browser();
  auto screen = ScreenInteractive::FitComponent();

  std::thread thr{[&] {
    while (cfg::active.get()) {
      cfg::registry().apply_update_and_check_if_dirty();
      std::this_thread::sleep_for(100ms);
    }

    screen.ExitLoopClosure()();
  }};

  screen.Loop(
      Renderer(component,
               [&] {
                 cfg::labels::foo.async_modify(cfg::labels::foo.get() + 1);
                 return window(text("< configs >"), component->Render())
                        | size(ftxui::HEIGHT, ftxui::LESS_THAN, 55);
               }));

  thr.join();
  return 0;
}

// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.