#pragma once
#include <memory>
#include <string_view>
#include <type_traits>

#include <perfkit/common/delegate.hxx>
#include <perfkit/common/hasher.hxx>
#include <perfkit/fwd.hpp>

#include "graphics.hpp"

namespace perfkit {
using window_ptr = std::shared_ptr<gui::instance>;

namespace gui {

/** Modal result */
enum class modal_result {
  abort,  // user closed window, or abort event raised.
  yes,
  no,
  refresh,
};

namespace events {
struct arg_mouse {
};

struct arg_keyboard {
};

struct arg_input {
};

class events {
 public:
  delegate<arg_mouse> clicked;
  delegate<arg_mouse> mouse_down;
  delegate<arg_mouse> mouse_move;
  delegate<arg_mouse> mouse_up;
  delegate<arg_keyboard> keypress;
  delegate<arg_keyboard> keyup;
  delegate<arg_input> input;
};
}  // namespace events

/**
 * Provides indirect access to internal buffer instance, for load balancing & interactions.
 */
class instance {
 public:
  /**
   * Wait until backend is ready, and render.
   * @tparam RenderFn_
   * @param render_fn
   * @return false if there's no graphics backend
   */
  template <typename RenderFn_,
            typename = std::enable_if_t<std::is_invocable_v<RenderFn_, buffer*>>>
  bool render(RenderFn_&& render_fn) {
    // TODO: call render_fn with buffer argument when ready
    if (not wait_backend_ready())
      return false;

    // TODO: call render_fn
    return true;
  }

  /**
   *
   * @tparam RenderFn_
   * @param render_fn
   * @return
   */
  template <typename RenderFn_,
            typename = std::enable_if_t<std::is_invocable_v<RenderFn_, buffer*>>>
  bool try_render(RenderFn_&& render_fn) {
    if (not is_open() || not is_backend_ready())
      return false;

    // TODO: call render_fn with buffer argument only when ready
    return true;
  }

  /**
   * Modal window
   *
   * @tparam RenderFn_
   * @param render_fn
   * @return
   */
  template <typename RenderFn_,
            typename = std::enable_if_t<std::is_invocable_v<RenderFn_, buffer*, modal_result*>>>
  modal_result modality(RenderFn_&& render_fn) {
    auto result = modal_result::refresh;
    while (result == modal_result::refresh)
      this->render([&](buffer* f) { render_fn(f, &result); });

    return result;
  }

  events::events* events() noexcept;

 public:
  bool is_open() const noexcept { return true; }
  bool is_backend_ready() const noexcept { return true; }

  bool wait_backend_ready() const noexcept;
};

/**
 * Holds list of instances
 */
class registry {
};

}  // namespace gui
}  // namespace perfkit