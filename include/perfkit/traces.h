#pragma once
#include "perfkit/detail/tracer.hpp"

#define INTERNAL_PERFKIT_TRACER_STRINGIFY2(X) #X
#define INTERNAL_PERFKIT_TRACER_STRINGIFY(X)  INTERNAL_PERFKIT_TRACER_STRINGIFY2(X)
#define INTERNAL_PERFKIT_TRACER_CONCAT2(X, Y) X##Y
#define INTERNAL_PERFKIT_TRACER_CONCAT(X, Y)  INTERNAL_PERFKIT_TRACER_CONCAT2(X, Y)

#define PERFKIT_TRACE_START(TracerPtr)                                         \
    auto INTERNAL_PERFKIT_TRACER_CONCAT(INTERNAL_PERFKIT_ROOT_TRACE, __LINE__) \
            = TracerPtr->fork(__func__)

#define PERFKIT_TRACE_FUNCTION(TracerPtr)                                      \
    auto INTERNAL_PERFKIT_TRACER_CONCAT(INTERNAL_PERFKIT_ROOT_TRACE, __LINE__) \
            = TracerPtr->timer(__func__)

#define PERFKIT_TRACE_SCOPE(TracerPtr, Name)     auto Name = TracerPtr->timer(#Name)
#define PERFKIT_TRACE_BLOCK(TracerPtr, Name)     if (PERFKIT_TRACE_SCOPE(TracerPtr, Name); true)
#define PERFKIT_TRACE_EXPR(TracerPtr, ValueExpr) TracerPtr->branch(#ValueExpr) = (ValueExpr);
