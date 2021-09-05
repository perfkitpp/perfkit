#include "perfkit/ftxui-extension.hpp"

#include "./config_browser.hpp"
#include "./trace_browser.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "perfkit/detail/configs.hpp"

ftxui::Component perfkit_ftxui::config_browser() {
  return std::shared_ptr<ftxui::ComponentBase>{new class config_browser};
}

ftxui::Component perfkit_ftxui::trace_browser() {
  return ftxui::Component();
}

class perfkit_ftxui::worker_info_t {
 public:
  ~worker_info_t() noexcept {
    _alive.store(false);
    if (thrd_loop.joinable()) { thrd_loop.join(); }
    if (thrd_poll.joinable()) { thrd_poll.join(); }
  }

 public:
  std::thread thrd_loop;
  std::thread thrd_poll;

  std::atomic_bool _alive{true};
};

perfkit_ftxui::kill_switch_ty perfkit_ftxui::launch_async_loop(
    std::shared_ptr<ftxui::ScreenInteractive> screen,
    ftxui::Component root_component,
    std::chrono::milliseconds poll_interval) {
  auto ptr = std::make_shared<worker_info_t>();

  ptr->thrd_loop = std::thread{
      [screen, root_component, flag = &ptr->_alive] {
        screen->Loop(root_component);
        flag->store(false, std::memory_order_relaxed);  // for when screen turned off first ...
      }};

  ptr->thrd_poll = std::thread{
      [screen, flag = &ptr->_alive, poll_interval] {
        using clock_ty                   = std::chrono::steady_clock;
        clock_ty::time_point next_wakeup = {};

        for (; flag->load(std::memory_order_relaxed);) {
          screen->PostEvent(event_poll);

          std::this_thread::sleep_until(next_wakeup);
          next_wakeup = clock_ty::now() + poll_interval;
        }
        screen->ExitLoopClosure()();
      }};

  return ptr;
}
