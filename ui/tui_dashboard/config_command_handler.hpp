#pragma once
#include "perfkit/detail/command_registry.hpp"
#include "perfkit/detail/configs.hpp"
#include "spdlog/fwd.h"

namespace perfkit::ui::detail {

class config_command_handler {
 public:
  struct init_args {
    spdlog::logger* logging;
    config_ptr      config;
    nlohmann::json  prototype;
  };

 public:
  explicit config_command_handler(init_args const& i);

  bool config(ui::args_view argv);

 private:
  spdlog::logger* _log;
  config_ptr      _conf;
  nlohmann::json  _proto;
};

}  // namespace perfkit::ui::detail