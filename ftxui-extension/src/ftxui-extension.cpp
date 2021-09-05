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
    ftxui::ScreenInteractive* screen,
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

namespace {
using namespace ftxui;

class atomic_string_queue : public perfkit_ftxui::string_queue {
 public:
  bool getline(std::string& string, std::chrono::milliseconds milliseconds) override {
    std::unique_lock _{_lock};
    if (!_queue.empty()) { return string = _pop(), true; }

    std::string result;
    return _cvar.wait_for(_, milliseconds,
                          [&result, this] {
                            if (_queue.empty()) { return false; }
                            result = _pop();
                            return true;
                          });
  }

 public:
  void putline(std::string&& str) {
    std::unique_lock _{_lock};
    _queue.emplace_back(std::move(str));
    _cvar.notify_one();
  }

 private:
  std::string _pop() {
    auto result = _queue.front();
    _queue.pop_front();
    return result;
  }

 private:
  std::condition_variable _cvar;
  std::mutex _lock;
  std::list<std::string> _queue;
};
}  // namespace

namespace perfkit {
namespace {
class _inputbox : public ComponentBase {
 public:
  explicit _inputbox(std::shared_ptr<atomic_string_queue> queue,
                     std::string const& prompt) {
    _queue = queue;

    InputOption opts;
    opts.cursor_position = &_cpos;
    opts.on_enter        = [this] { _on_enter(); };
    _box                 = Input(&_cmd, prompt, opts);

    Add(_box);
  }

 private:
  void _on_enter() {
    _queue->putline(ftxui::to_string(_cmd));
    _cmd.clear();
  }

 public:
  Element Render() override {
    return _box->Render() | xflex;
  }

 private:
  std::shared_ptr<atomic_string_queue> _queue;
  int _cpos = {};
  std::wstring _cmd;
  Component _box;
};
}  // namespace
}  // namespace perfkit

ftxui::Component perfkit_ftxui::command_input(
    std::shared_ptr<perfkit_ftxui::string_queue>* out_supplier,
    std::weak_ptr<perfkit::ui::command_registry> support,
    std::string prompt) {
  auto ptr      = std::make_shared<atomic_string_queue>();
  *out_supplier = ptr;

  return std::make_shared<_inputbox>(ptr, std::move(prompt));
}

ftxui::Component perfkit_ftxui::PRESET(
    std::shared_ptr<string_queue>* out_commands,
    std::weak_ptr<perfkit::ui::command_registry> command_support,
    std::shared_ptr<if_subscriber> subscriber) {
  auto cmd   = command_input(out_commands, command_support, ":> enter command");
  auto cfg   = config_browser();
  auto trace = trace_browser(subscriber);
  // TODO: in future, put log output window ...

  auto resize_context = std::make_shared<int>(30);
  auto components     = ResizableSplitLeft(cfg, trace, resize_context.get());

  components = CatchEvent(components, [components](Event evt) {
    if (evt == EVENT_POLL) {
      for (size_t j = 0; j < components->ChildAt(0)->ChildCount(); ++j) {
        components->ChildAt(0)->ChildAt(j)->OnEvent(evt);
      }
    }
    return false;
  });

  components = Container::Vertical({
      Renderer(cmd, [cmd] { return window(text("< commands >"), cmd->Render()); }),
      Renderer(components, [components] { return components->Render() | border | flex; }),
  });

  return event_dispatcher(components);
}

bool perfkit_ftxui::is_alive(const perfkit_ftxui::worker_info_t* kill_switch) {
  return kill_switch->_alive.load(std::memory_order_relaxed);
}
