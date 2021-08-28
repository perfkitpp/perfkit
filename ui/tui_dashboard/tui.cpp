//
// Created by Seungwoo on 2021-08-28.
//
#include "tui.hpp"

#include <range/v3/algorithm.hpp>
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>

namespace {
// automatically registers backend.
auto REGISTER = perfkit::detail::register_function(
    perfkit::ui::basic_backend::tui_dashboard,
    [](auto s) -> perfkit::ui::if_ui* { return new perfkit::detail::dashboard{s}; });
}  // namespace

using namespace perfkit::detail;

perfkit::detail::dashboard::dashboard(perfkit::array_view<std::string_view>) {
}

void perfkit::detail::dashboard::poll(bool do_content_fetch) {
  // render command view

  // if content fetch, do option view

  // if

}

auto perfkit::detail::dashboard::commands() -> perfkit::ui::command_register::node* {
  return nullptr;
}

void perfkit::detail::dashboard::launch(std::chrono::milliseconds interval, int content_fetch_cycle) {
}

void perfkit::detail::dashboard::invoke_command(std::string_view view) {
}

void perfkit::detail::dashboard::stop() {
}
