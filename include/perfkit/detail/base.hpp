#pragma once
#include <memory>
#include <string>
#include <string_view>

#include <spdlog/fwd.h>

namespace perfkit {
std::shared_ptr<spdlog::logger> glog();

}  // namespace perfkit