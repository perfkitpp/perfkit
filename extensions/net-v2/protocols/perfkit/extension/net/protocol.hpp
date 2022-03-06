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

#pragma once
#include "perfkit/common/refl/core.hxx"
#include "perfkit/common/refl/extension/msgpack-rpc/stub.hxx"

namespace perfkit::msgpack::rpc {
class context;
}

namespace perfkit::net::message {
using std::string;
using std::vector;

#define DEFINE_STUB(Name, ...) \
    static inline const auto Name = msgpack::rpc::create_signature<__VA_ARGS__>(#Name);

struct session_state
{
    CPPH_REFL_DECLARE_c;

    bool va;

    double cpu_usage_total_user;
    double cpu_usage_total_system;
    double cpu_usage_self_user;
    double cpu_usage_self_system;

    int64_t memory_usage_virtual;
    int64_t memory_usage_resident;
    int32_t num_threads;

    int32_t bw_out;
    int32_t bw_in;
};

//
// SHELL
//
class notify
{
    //! Shell content output
    DEFINE_STUB(tty, void(int64_t fence, string const& content));

    //! Configs update

    //! Trace update

    //!
};

class service
{
    struct fetch_tty_result_t
    {
        CPPH_REFL_DECLARE_c;

        int64_t fence = 0;
        string content;
    };

    DEFINE_STUB(fetch_tty, fetch_tty_result_t(int64_t fence));
};

#undef DEFINE_STUB
}  // namespace perfkit::net::message
