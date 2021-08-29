//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <spdlog/fwd.h>

#include <filesystem>
#include <iostream>
#include <perfkit/detail/options.hpp>
#include <perfkit/detail/tracer.hpp>

namespace perfkit {
std::shared_ptr<spdlog::logger> glog();

bool import_options(std::filesystem::path src);
void export_options(std::filesystem::path dst);
}  // namespace perfkit

#define PERFKIT_OPTION_DISPATCHER(Name) static inline auto& Name = perfkit::option_registry::_create()
#define PERFKIT_OPTION                  static inline perfkit::option

#define PERFKIT_MESSAGE_BLOCK static inline perfkit::tracer
