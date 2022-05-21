/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2022. Seungwoo Kang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * project home: https://github.com/perfkitpp
 ******************************************************************************/

#pragma once

#include "configs-v2.hpp"

namespace perfkit::v2 {
struct config_parse_arg_option {
    bool consume_flags = true;
    bool collect_unregistered_registerise = true;
    bool update_collected_registries = true;
    bool allow_flag_duplication = true;
    bool allow_unknown_flag = true;
    bool is_first_arg_exec_name = true;
};

// TODO:
//   parse_args default takes list of active config options or config registries
//   if nothing is specified, parse_args will collect all globally registered
//    config registry instances, and will create flag binding table, then perform
//    args parsing.
//
void configs_parse_args(
        int& ref_argc, char const**& ref_argv,
        config_parse_arg_option option = {},
        array_view<config_registry_ptr> regs = {});
}  // namespace perfkit::v2
