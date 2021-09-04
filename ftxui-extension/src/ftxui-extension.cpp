#include "perfkit/ftxui-extension.hpp"

#include "./config_browser.hpp"
#include "perfkit/detail/configs.hpp"

ftxui::Component perfkit_fxtui::config_browser() {
  return std::shared_ptr<ftxui::ComponentBase>{new perfkit_ftxui::config_browser};
}
