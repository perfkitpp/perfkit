//
// Created by Seungwoo on 2021-09-25.
//

#pragma once
#include <future>

#include "tracer.hpp"

namespace perfkit {
class tracer_future_result : public std::shared_future<tracer::async_trace_result>
{
    using shared_future::shared_future;

   public:
    template <class Rty_>
    auto& operator=(Rty_&& r) noexcept
    {
        *(std::shared_future<tracer::async_trace_result>*)this = std::forward<Rty_>(r);
        return *this;
    }
};

}  // namespace perfkit