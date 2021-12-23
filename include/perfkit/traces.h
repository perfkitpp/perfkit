#pragma once
#include "perfkit/common/utility/ownership.hxx"
#include "perfkit/detail/tracer.hpp"

#define INTERNAL_PERFKIT_TRACER_STRINGIFY2(X) #X
#define INTERNAL_PERFKIT_TRACER_STRINGIFY(X)  INTERNAL_PERFKIT_TRACER_STRINGIFY2(X)
#define INTERNAL_PERFKIT_TRACER_CONCAT2(X, Y) X##Y
#define INTERNAL_PERFKIT_TRACER_CONCAT(X, Y)  INTERNAL_PERFKIT_TRACER_CONCAT2(X, Y)

#define PERFKIT_TRACE_DECLARE(TracerPtr)               \
    auto INTERNAL_PERFKIT_ACTIVE_TRACER = &*TracerPtr; \
    auto INTERNAL_PERFKIT_SEQ_TRACE     = ::perfkit::tracer_proxy::_bk_create_default()

#define PERFKIT_TRACE(TracerPtr)                                               \
    auto INTERNAL_PERFKIT_TRACER_CONCAT(INTERNAL_PERFKIT_ROOT_TRACE, __LINE__) \
            = TracerPtr->fork(__func__);                                       \
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

#define PERFKIT_TRACE_DATA(String, ValueExpr) \
    INTERNAL_PERFKIT_ACTIVE_TRACER->branch(String, (ValueExpr))

#define PERFKIT_TRACE_BRANCH(String) \
    INTERNAL_PERFKIT_ACTIVE_TRACER->branch(String)

#define PERFKIT_TRACE_SEQUENCE(Name)                                               \
    if (not INTERNAL_PERFKIT_SEQ_TRACE.is_valid())                                 \
    {                                                                              \
        INTERNAL_PERFKIT_SEQ_TRACE = INTERNAL_PERFKIT_ACTIVE_TRACER->timer(#Name); \
    }                                                                              \
    else                                                                           \
    {                                                                              \
        INTERNAL_PERFKIT_SEQ_TRACE.switch_to_timer(#Name);                         \
    }                                                                              \
    auto &Name = INTERNAL_PERFKIT_SEQ_TRACE
