#include "ftxui/component/captured_mouse.hpp"      // for ftxui
#include "ftxui/component/component.hpp"           // for Checkbox, Vertical
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "perfkit/configs.h"
#include "perfkit/ftxui-extension.hpp"
#include "perfkit/traces.h"
#include "spdlog/fmt/fmt.h"

using namespace std::literals;

std::map<std::string, std::map<std::string, std::string>> ved{
        {"asd", {{"asd", "weqw"}, {"vafe, ewqew", "dwrew"}}},
        {"vadsfew", {{"dav ,ea w", "Ewqsad"}, {"scxz ss", "dwqewqew"}}}};

PERFKIT_CATEGORY(cfg) {
  PERFKIT_CONFIGURE(active, true).confirm();
  PERFKIT_CONFIGURE(active_async, true).confirm();
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

PERFKIT_CATEGORY(vlao) {
  PERFKIT_CONFIGURE(e_cedrs, 1).confirm();
  PERFKIT_CONFIGURE(e_cedrsd, "").confirm();
}
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

perfkit::tracer traces[] = {
        {0, "root (1)"},
        {1, "A (2)"},
        {31, "B (4)"},
        {-51, "C (0)"},
        {14, "D (3)"},
};

class my_subscriber : public perfkit_ftxui::if_subscriber {
 public:
  bool on_update(const update_param_type& param_type, const perfkit::trace_variant_type& value) override {
    traces[1].fork("Value update A")["NAME"] = param_type.name;
    return true;
  }

  void on_end(const update_param_type& param_type) override {
    vlao::e_cedrs.async_modify(vlao::e_cedrs.get() + 1);
    vlao::e_cedrsd.async_modify(std::string(param_type.name));
    vlao::registry().apply_update_and_check_if_dirty();
  }
};

int main(int argc, const char* argv[]) {
  auto screen = ScreenInteractive::Fullscreen();

  std::shared_ptr<perfkit_ftxui::string_queue> commands;
  auto preset      = perfkit_ftxui::PRESET(&commands, {}, std::make_shared<my_subscriber>());
  auto kill_switch = perfkit_ftxui::launch_async_loop(&screen, preset);

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

    std::string to_get;
    if (commands->try_getline(to_get)) {
      trc_root["TEXT"] = to_get;
    }

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