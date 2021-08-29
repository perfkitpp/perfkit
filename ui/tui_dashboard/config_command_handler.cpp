//
// Created by Seungwoo on 2021-08-30.
//
#include "config_command_handler.hpp"

#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"

using namespace perfkit;
using namespace perfkit::ui;

bool ui::detail::config_command_handler::config(ui::args_view argv) {
  if (argv.empty()) {
    fmt::print("\"{}\": {}\n", _conf->display_key(), _conf->serialize().dump(4, ' '));
    return true;
  }
  
  if (argv.size() == 2 && argv[0] == "-r") {
    std::string argval;
    if (_proto.is_string()) {
      argval.append("\"").append(argv[1]).append("\"");
    } else {
      argval.append(argv[1]);
    }

    auto json = nlohmann::json ::parse(argval, nullptr, false);

    if (json.is_discarded()) {
      _log->error("invalid json syntax.");
      return false;
    }

    if (!!strcmp(json.type_name(), _proto.type_name())) {
      _log->error("type mismatch - given: '{}', should be: '{}'",
                  json.type_name(), _proto.type_name());
      return false;
    }

    config_registry::request_update_value(std::string(_conf->full_key()), json);
    return true;
  }

  return false;
}

ui::detail::config_command_handler::config_command_handler(
    const detail::config_command_handler::init_args& i)
    : _log(i.logging)
    , _conf(i.config)
    , _proto(i.prototype) {
}
