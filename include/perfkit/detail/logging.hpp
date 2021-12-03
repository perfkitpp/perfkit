//
// Created by ki608 on 2021-11-27.
//
#pragma once
#include <memory>
#include <string>

#include <perfkit/fwd.hpp>

namespace perfkit
{

using logger_ptr = std::shared_ptr<spdlog::logger>;
logger_ptr share_logger(std::string const& name);

// TODO: logic for enumerate loggers, save/load logging status, etc ...

}  // namespace perfkit
