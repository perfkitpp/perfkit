#pragma once
#include <chrono>
#include <string_view>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "perfkit/detail/array_view.hxx"

namespace ftxui {
class ScreenInteractive;
};

namespace perfkit {
struct trace_variant_type;
};

namespace perfkit_ftxui {

using namespace std::literals;
static inline const auto EVENT_POLL = ftxui::Event::Special("POLL");

class if_subscriber {
 public:
  struct update_param_type {
    std::string_view block_name;

    uint64_t hash = {};
    perfkit::array_view<std::string_view> hierarchy;
    std::string_view name;
  };

 public:
  virtual ~if_subscriber();

  virtual bool on_update(
      update_param_type const& param_type,
      perfkit::trace_variant_type const& value)
      = 0;

  virtual void on_end(update_param_type const& param_type) = 0;
};

ftxui::Component config_browser();
ftxui::Component trace_browser(std::shared_ptr<if_subscriber> m);

using exclusive_file_ptr = std::unique_ptr<FILE*, void (*)(FILE*)>;
ftxui::Component log_output_window(exclusive_file_ptr stream);
ftxui::Component cmd_line_window(std::function<void(std::string_view)> on_cmd);
ftxui::Component event_dispatcher(ftxui::Component, const ftxui::Event& evt_type = EVENT_POLL);

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