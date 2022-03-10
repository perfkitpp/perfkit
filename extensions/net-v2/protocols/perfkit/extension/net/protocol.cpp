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

//
// Created by ki608 on 2022-03-06.
//

#include "protocol.hpp"

#include "perfkit/common/refl/container/binary.hxx"
#include "perfkit/common/refl/container/list.hxx"
#include "perfkit/common/refl/object.hxx"

namespace perfkit::net::message {
CPPH_REFL_DEFINE_OBJECT_c(
        notify::session_state_t, (),
        (cpu_usage_total_user, 1), (cpu_usage_total_system, 2),
        (cpu_usage_self_user, 3), (cpu_usage_self_system, 4),
        (memory_usage_virtual, 11), (memory_usage_resident, 12),
        (num_threads, 21),
        (bw_out, 31), (bw_in, 32));

CPPH_REFL_DEFINE_OBJECT_c(
        tty_output_t, (),
        (fence, 1), (content, 2));

CPPH_REFL_DEFINE_OBJECT_c(
        config_entity_t, (),
        (name, 1), (config_key, 2),
        (description, 11),
        (opt_min, 101), (opt_max, 102), (opt_one_of, 103));

CPPH_REFL_DEFINE_OBJECT_c(
        config_category_t, (),
        (name, 1),
        (subcategories, 2),
        (entities, 3));

CPPH_REFL_DEFINE_OBJECT_c(
        notify::config_update_t, (),
        (config_key, 1), (content_next, 2));
}  // namespace perfkit::net::message
