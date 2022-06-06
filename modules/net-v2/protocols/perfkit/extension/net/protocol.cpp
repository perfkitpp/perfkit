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

#include "cpph/refl/object.hxx"
#include "cpph/refl/types/array.hxx"
#include "cpph/refl/types/binary.hxx"
#include "cpph/refl/types/chrono.hxx"
#include "cpph/refl/types/list.hxx"
#include "cpph/refl/types/tuple.hxx"
#include "cpph/refl/types/variant.hxx"

namespace perfkit::net::message {
CPPH_REFL_DEFINE_OBJECT_c(
        notify::session_status_t, (),
        (cpu_usage_total_user, 1), (cpu_usage_total_system, 2),
        (cpu_usage_self_user, 3), (cpu_usage_self_system, 4),
        (memory_usage_virtual, 11), (memory_usage_resident, 12),
        (num_threads, 21),
        (bw_out, 31), (bw_in, 32));

CPPH_REFL_DEFINE_OBJECT_c(
        service::session_info_t, (),
        (name, 1), (hostname, 2), (keystr, 3),
        (epoch, 4), (description, 5), (num_cores, 6));

CPPH_REFL_DEFINE_OBJECT_c(
        tty_output_t, (),
        (fence, 1), (content, 2));

CPPH_REFL_DEFINE_OBJECT_c(
        config_entity_t, (),
        (name, 1), (config_key, 2),
        (description, 11), (initial_value, 12),
        (opt_min, 101), (opt_max, 102), (opt_one_of, 103));

CPPH_REFL_DEFINE_OBJECT_c(
        notify::config_category_t, (),
        (name, 1),
        (subcategories, 2),
        (entities, 3),
        (category_id, 4));

CPPH_REFL_DEFINE_OBJECT_c(
        config_entity_update_t, (),
        (config_key, 1), (content_next, 2));

CPPH_REFL_DEFINE_OBJECT_c(
        service::suggest_result_t, (),
        (replace_range, 1), (replaced_content, 2), (candidate_words, 3));

CPPH_REFL_DEFINE_OBJECT_c(
        tracer_descriptor_t, (),
        (tracer_id, 1), (name, 2), (priority, 3), (epoch, 4));

CPPH_REFL_DEFINE_OBJECT_c(
        trace_info_t, (),
        (name, 1), (hash, 11), (owner_tracer_id, 2), (index, 3), (parent_index, 4));

CPPH_REFL_DEFINE_OBJECT_c(
        trace_update_t, (),
        (index, 4), (fence_value, 1), (occurrence_order, 2), (flags, 3), (payload, 5));

CPPH_REFL_DEFINE_OBJECT_c(
        service::trace_control_t, (), (subscribe, 2), (fold, 3));

CPPH_REFL_DEFINE_OBJECT_c(
        find_me_t, (), (alias, 1), (port, 2));

}  // namespace perfkit::net::message
