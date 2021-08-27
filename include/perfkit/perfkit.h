//
// Created by Seungwoo on 2021-08-25.
//
#pragma once
#include <iostream>
#include <perfkit/detail/message_block.hpp>
#include <perfkit/detail/options.hpp>

namespace perfkit {
bool read_options(std::istream& src);
void dump_options(std::ostream& dst);
}  // namespace perfkit

#define PERFKIT_OPTION_DISPATCHER(Name) static inline auto& Name = perfkit::option_dispatcher::_create()
#define PERFKIT_OPTION                  static inline perfkit::option

#define PERFKIT_MESSAGE_BLOCK static inline perfkit::message_block