#pragma once
#include <memory>

#include <perfkit/common/helper/spdlog_macros.hxx>
#include <perfkit/fwd.hpp>

namespace perfkit {
using logger_ptr = std::shared_ptr<spdlog::logger>;
}  // namespace perfkit

namespace perfkit::logging {
/**
 * Try get logger, and if logger with given key does not exist, clone default logger then return.
 *
 * @param name
 * @return
 */
logger_ptr find_or(std::string const& name);
}  // namespace perfkit::logging
