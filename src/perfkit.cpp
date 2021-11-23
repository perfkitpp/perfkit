//
// Created by Seungwoo on 2021-08-29.
//
#include "perfkit/perfkit.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <perfkit/detail/base.hpp>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

std::string perfkit::_configs_internal::INDEXER_STR(int order)
{
    return fmt::format("{:->5}", order);
}

std::shared_ptr<spdlog::logger> perfkit::glog()
{
    static std::weak_ptr<spdlog::logger> _inst{};
    if (!_inst.lock())
    {
        auto ptr = spdlog::get("PERFKIT");
        if (!ptr) { ptr = spdlog::stdout_color_mt("PERFKIT"); }
        _inst = ptr;
    }

    return _inst.lock();
}
