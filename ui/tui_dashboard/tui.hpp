//
// Created by Seungwoo on 2021-08-28.
//
#pragma once
#include "curses_local.h"
#include "perfkit/perfkit.h"
#include "perfkit/ui.hpp"
#include "ui-tools.hpp"

namespace perfkit::detail {
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
};
}  // namespace perfkit::detail