#include "ftxui/component/captured_mouse.hpp"      // for ftxui
#include "ftxui/component/component.hpp"           // for Checkbox, Vertical
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "perfkit/configs.h"
#include "perfkit/ftxui-extension.hpp"
#include "perfkit/traces.h"
#include "spdlog/fmt/fmt.h"
#include "test_configs.hxx"

using namespace ftxui;

perfkit::tracer traces[] = {
        {0, "root (1)"},
        {1, "A (2)"},
        {31, "B (4)"},
        {-51, "C (0)"},
        {14, "D (3)"},
};

int main(int argc, const char* argv[]) {
  auto comp1     = perfkit_ftxui::config_browser();
  auto comp2     = perfkit_ftxui::trace_browser(nullptr);
  auto component = ftxui::Container::Horizontal({
          comp1,
          Renderer([] { return separator(); }),
          comp2,
  });
  component      = perfkit_ftxui::event_dispatcher(component);

  auto screen = ScreenInteractive::Fullscreen();

  auto kill_switch = perfkit_ftxui::launch_async_loop(
          &screen,
          CatchEvent(
                  Renderer(
                          component,
                          [&] {
                            return window(text("< configs >"), component->Render())
                                 | size(ftxui::HEIGHT, ftxui::LESS_THAN, 55);
                          }),
                  [p = &screen](Event evt) -> bool {
                    if (evt == perfkit_ftxui::EVENT_POLL) {
                      if (cfg::active.get() == false) {
                        p->ExitLoopClosure()();
                      }
                    }
                    return false;
                  }),
          50ms);

  for (int ic = 0; perfkit_ftxui::is_alive(kill_switch.get()); ++ic) {
    std::this_thread::sleep_for(10ms);
    cfg::registry().apply_update_and_check_if_dirty();

    auto trc_root = traces[0].fork("Root Trace");

    auto timer                         = trc_root.timer("Some Timer");
    trc_root["Value 0"]                = 3;
    trc_root["Value 1"]                = *cfg::labels::foo;
    trc_root["Value 2"]                = fmt::format("Hell, world! {}", *cfg::labels::foo);
    trc_root["Value 3"]                = false;
    trc_root["Value 3"]["Subvalue 0"]  = ic;
    trc_root["Value 3"]["Subvalue GR"] = std::vector<int>{3, 4, 5};
    trc_root["Value 3"]["Subvalue 1"]  = double(ic);
    trc_root["Value 3"]["Subvalue 2"]  = !!(ic & 1);
    trc_root["Value 4"]["Subvalue 3"]  = fmt::format("Hell, world! {}", ic);

    auto r                            = trc_root["Value 5"];
    trc_root["Value 5"]["Subvalue 0"] = ic;
    if (r) { trc_root["Value 5"]["Subvalue 1 Cond"] = double(ic); }
    trc_root["Value 5"]["Subvalue 2"] = !!(ic & 1);

    cfg::labels::foo.async_modify(cfg::labels::foo.get() + 1);
    if (cfg::active_async.get() == false) {
      kill_switch.reset();
      break;
    }
  }

  return 0;
}

// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.