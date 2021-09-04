#pragma once

namespace perfkit_curses {

class panel {
 public:
  virtual ~panel() noexcept = default;

};

class config_panel {
};

class logging_panel {
};

class trace_panel {
};

class watched_trace_panel {
};
}  // namespace perfkit_curses