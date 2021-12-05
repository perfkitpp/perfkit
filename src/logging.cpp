#include "perfkit/detail/logging.hpp"

#include <spdlog/spdlog.h>

perfkit::logger_ptr perfkit::share_logger(std::string const& name)
{
    auto ptr = spdlog::get(name);

    if (not ptr)
    {
        ptr = spdlog::default_logger()->clone(name);
    }

    return ptr;
}
