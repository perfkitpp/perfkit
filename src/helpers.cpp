#include <perfkit/detail/helpers.hpp>
#include <spdlog/spdlog.h>

namespace perfkit {
logger_ptr logging::find_or(const std::string& name) {
  auto logger = spdlog::get(name);
  if (not logger) {
    logger = spdlog::default_logger()->clone(name);
    spdlog::register_logger(logger);
  }

  return logger;
}
}  // namespace perfkit
