#include "perfkit/ftxui-extension.hpp"

#include "./config_browser.hpp"
#include "./trace_browser.hpp"
#include "perfkit/detail/configs.hpp"

ftxui::Component perfkit_fxtui::config_browser() {
  return std::shared_ptr<ftxui::ComponentBase>{new perfkit_ftxui::config_browser};
}

  ftxui::Component perfkit_fxtui::trace_browser() {
  return ftxui::Component();
}
