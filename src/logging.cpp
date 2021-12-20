#include "perfkit/detail/logging.hpp"

#include <perfkit/common/spinlock.hxx>
#include <spdlog/spdlog.h>

perfkit::logger_ptr perfkit::share_logger(std::string const& name)
{
    static perfkit::spinlock mtx;
    std::lock_guard _{mtx};

    auto ptr = spdlog::get(name);

    if (not ptr)
    {
        ptr = spdlog::default_logger()->clone(name);
        spdlog::register_logger(ptr);
    }

    return ptr;
}
