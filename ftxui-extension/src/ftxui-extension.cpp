#include "perfkit/ftxui-extension.hpp"

#include "./config_browser.hpp"
#include "./trace_browser.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "perfkit/detail/configs.hpp"

ftxui::Component perfkit_ftxui::config_browser() {
  return std::shared_ptr<ftxui::ComponentBase>{new class config_browser};
}

ftxui::Component perfkit_ftxui::trace_browser(std::shared_ptr<if_subscriber> m) {
  return std::shared_ptr<ftxui::ComponentBase>{new class trace_browser(std::move(m))};
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
          screen->PostEvent(EVENT_POLL);

          std::this_thread::sleep_until(next_wakeup);
          next_wakeup = clock_ty::now() + poll_interval;
        }
        screen->ExitLoopClosure()();
      }};

  return ptr;
}

namespace {
class EventCatcher : public ftxui::ComponentBase {
 public:
  EventCatcher(ftxui::Component child, ftxui::Event const& evt_type)
      : _root(child), _evt_type(evt_type) {
    Add(_root);
  }

  bool OnEvent(ftxui::Event event) override {
    if (event == _evt_type) {
      for (size_t index = 0; index < _root->ChildCount(); ++index) {
        _root->ChildAt(index)->OnEvent(event);
      }

      return false;
    } else {
      return _root->OnEvent(event);
    }
  }

 private:
  ftxui::Component _root;
  ftxui::Event _evt_type;
};
}  // namespace

ftxui::Component perfkit_ftxui::event_dispatcher(ftxui::Component c, const Event& evt_type) {
  return std::static_pointer_cast<ftxui::ComponentBase>(
      std::make_shared<EventCatcher>(c, evt_type));
}
