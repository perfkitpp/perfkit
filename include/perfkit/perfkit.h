//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <iostream>
#include <perfkit/detail/messenger.hpp>
#include <perfkit/detail/options.hpp>

namespace perfkit {
bool import_options(std::istream& src);
void export_options(std::ostream& dst);
}  // namespace perfkit

#define PERFKIT_OPTION_DISPATCHER(Name) static inline auto& Name = perfkit::option_registry::_create()
#define PERFKIT_OPTION                  static inline perfkit::option

#define PERFKIT_MESSAGE_BLOCK static inline perfkit::messenger
