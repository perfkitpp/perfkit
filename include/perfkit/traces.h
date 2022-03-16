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
#include "perfkit/common/utility/ownership.hxx"
#include "perfkit/detail/tracer.hpp"

#define INTERNAL_PERFKIT_TRACER_STRINGIFY2(X) #X
#define INTERNAL_PERFKIT_TRACER_STRINGIFY(X)  INTERNAL_PERFKIT_TRACER_STRINGIFY2(X)
#define INTERNAL_PERFKIT_TRACER_CONCAT2(X, Y) X##Y
#define INTERNAL_PERFKIT_TRACER_CONCAT(X, Y)  INTERNAL_PERFKIT_TRACER_CONCAT2(X, Y)

#define PERFKIT_RVAR auto INTERNAL_PERFKIT_TRACER_CONCAT(INTERNAL_PERFKIT_RVAR_, __LINE__)

#define PERFKIT_TRACE_DECLARE(TracerPtr)               \
    auto INTERNAL_PERFKIT_ACTIVE_TRACER = &*TracerPtr; \
    auto INTERNAL_PERFKIT_SEQ_TRACE     = ::perfkit::tracer_proxy::create_default()

#define PERFKIT_TRACE(TracerPtr, ...)                                          \
    auto INTERNAL_PERFKIT_TRACER_CONCAT(INTERNAL_PERFKIT_ROOT_TRACE, __LINE__) \
            = TracerPtr->fork(__func__, ##__VA_ARGS__);                        \
    PERFKIT_TRACE_DECLARE(TracerPtr)

#define PERFKIT_TRACE_FUNCTION(TracerPtr)                                      \
    auto INTERNAL_PERFKIT_TRACER_CONCAT(INTERNAL_PERFKIT_ROOT_TRACE, __LINE__) \
            = TracerPtr->timer(__func__);                                      \
    PERFKIT_TRACE_DECLARE(TracerPtr)

#define PERFKIT_TRACE_SCOPE(Name) \
    auto Name = INTERNAL_PERFKIT_ACTIVE_TRACER->timer(#Name)

#define PERFKIT_TRACE_BLOCK(Name) \
    if (PERFKIT_TRACE_SCOPE(Name); true)

#define PERFKIT_TRACE_EXPR(ValueExpr) \
    INTERNAL_PERFKIT_ACTIVE_TRACER->branch(#ValueExpr, (ValueExpr))

#define PERFKIT_TRACE_DATA(...) \
    INTERNAL_PERFKIT_ACTIVE_TRACER->branch(__VA_ARGS__)

#define PERFKIT_TRACE_BRANCH(String) \
    INTERNAL_PERFKIT_ACTIVE_TRACER->branch(String)

#define PERFKIT_TRACE_SEQUENCE(Name)                                               \
    if (not INTERNAL_PERFKIT_SEQ_TRACE.is_valid()) {                               \
        INTERNAL_PERFKIT_SEQ_TRACE = INTERNAL_PERFKIT_ACTIVE_TRACER->timer(#Name); \
    } else {                                                                       \
        INTERNAL_PERFKIT_SEQ_TRACE.switch_to_timer(#Name);                         \
    }                                                                              \
    auto& Name = INTERNAL_PERFKIT_SEQ_TRACE
