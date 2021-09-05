#pragma once
#include <chrono>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"

namespace ftxui {
class ScreenInteractive;
};

namespace perfkit_ftxui {

using namespace std::literals;
static inline const auto EVENT_POLL = ftxui::Event::Special("POLL");

ftxui::Component config_browser();
ftxui::Component trace_browser();

using exclusive_file_ptr = std::unique_ptr<FILE*, void (*)(FILE*)>;
ftxui::Component log_output_window(exclusive_file_ptr stream);
ftxui::Component cmd_line_window(std::function<void(std::string_view)> on_cmd);
ftxui::Component event_dispatcher(ftxui::Component, ftxui::Event evt_type = EVENT_POLL);

class worker_info_t;
using kill_switch_ty = std::shared_ptr<worker_info_t>;

/**
 * @param screen
 * @param root_component
 * @param poll_interval Interval to generate every Event::Special("POLL")
 * @return kill switch. To stop worker threads gently, release given pointer.
 */
kill_switch_ty launch_async_loop(
    std::shared_ptr<ftxui::ScreenInteractive> screen,
    ftxui::Component root_component,
    std::chrono::milliseconds poll_interval = 0ms);
}  // namespace perfkit_ftxui