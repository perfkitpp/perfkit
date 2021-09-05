#pragma once
#include "ftxui/component/component.hpp"

namespace perfkit_fxtui {

ftxui::Component config_browser();
ftxui::Component trace_browser();

using exclusive_file_ptr = std::unique_ptr<FILE*, void (*)(FILE*)>;
ftxui::Component log_output_window(exclusive_file_ptr stream);

ftxui::Component cmd_line_window(std::function<void(std::string_view)> on_cmd);
}  // namespace perfkit_fxtui