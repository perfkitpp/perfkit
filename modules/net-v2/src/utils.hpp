// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

#pragma once
#include <functional>
#include <memory>
#include <string>

#include <spdlog/fwd.h>

namespace perfkit::net::detail {
std::string try_fetch_input(int ms_to_wait);
spdlog::logger* nglog();

struct proc_stat_t {
    double cpu_usage_total_user;
    double cpu_usage_total_system;
    double cpu_usage_self_user;
    double cpu_usage_self_system;

    int64_t memory_usage_virtual;
    int64_t memory_usage_resident;
    int32_t num_threads;
};

bool fetch_proc_stat(proc_stat_t* ostat);

void input_redirect(std::function<void(char const*, size_t)> inserter);
void input_rollback();

void write(char const* buffer, size_t n);
}  // namespace perfkit::net::detail
