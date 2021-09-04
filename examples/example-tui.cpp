#include "ftxui/component/captured_mouse.hpp"      // for ftxui
#include "ftxui/component/component.hpp"           // for Checkbox, Vertical
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "perfkit/configs.h"
#include "perfkit/ftxui-extension.hpp"

PERFKIT_CATEGORY(cfg) {
  PERFKIT_SUBCATEGORY(labels) {
    PERFKIT_CONFIGURE(foo, 1).confirm();
    PERFKIT_CONFIGURE(bar, 1).confirm();
    PERFKIT_CONFIGURE(ce, 1).confirm();
    PERFKIT_CONFIGURE(ced, 1).confirm();
    PERFKIT_CONFIGURE(cedr, 1).confirm();
    PERFKIT_CONFIGURE(cedrs, 1).confirm();
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

using namespace ftxui;

int main(int argc, const char* argv[]) {
  auto component = perfkit_fxtui::config_browser();

  auto screen = ScreenInteractive::Fullscreen();
  screen.Loop(
      Renderer(component,
               [&] {
                 return component->Render() | size(ftxui::HEIGHT, ftxui::LESS_THAN, 10);
               }));

  return 0;
}

// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.