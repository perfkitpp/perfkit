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

#include "perfkit/common/refl/object.hxx"

namespace perfkit::net::message {
CPPH_REFL_DEFINE_OBJECT_c(
        session_state, (),
        (cpu_usage_total_user, 0),
        (cpu_usage_total_system, 1),
        (cpu_usage_self_user, 2),
        (cpu_usage_self_system, 3),
        (memory_usage_virtual, 4),
        (memory_usage_resident, 5),
        (num_threads, 6),
        (bw_out, 7),
        (bw_in, 8));

CPPH_REFL_DEFINE_OBJECT_c(
        service::fetch_tty_result_t, (),
        (fence, 0), (content, 1));
}  // namespace perfkit::net::message
