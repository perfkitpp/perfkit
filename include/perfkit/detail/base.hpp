#pragma once
#include <string>
#include <string_view>

#include <spdlog/fwd.h>

namespace perfkit {
std::shared_ptr<spdlog::logger> glog();

std::string dump_log_levels();              // TODO
void parse_log_levels(std::string const&);  // TODO

std::string dump_trace_status();       // TODO
void parse_trace_status(std::string);  // TODO

bool import_options(std::string_view src);
bool export_options(std::string_view dstpath);

}  // namespace perfkit