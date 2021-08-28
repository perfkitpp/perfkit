//
// Created by Seungwoo on 2021-08-28.
//
#pragma once
#include "perfkit/ui.hpp"

namespace perfkit::detail {

using cb_if_factory = ui::if_ui* (*)(array_view<std::string_view>);

nullptr_t register_function(
    ui::basic_backend b,
    cb_if_factory     factory);

}  // namespace perfkit::detail