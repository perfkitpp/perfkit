//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <perfkit/configs.h>
#include <perfkit/traces.h>
#include <spdlog/fwd.h>

namespace perfkit {
std::shared_ptr<spdlog::logger> glog();

bool import_options(std::string_view src);
bool export_options(std::string_view dstpath);

}  // namespace perfkit
