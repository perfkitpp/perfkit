#pragma once
#include <functional>
#include <memory>
#include <string>

#include <spdlog/fwd.h>

#include "perfkit/common/utility/inserter.hxx"

namespace perfkit::terminal::net::detail {
std::string try_fetch_input(int ms_to_wait);
std::shared_ptr<spdlog::logger> nglog();

struct proc_stat_t
{
    double cpu_usage_total_user;
    double cpu_usage_total_system;
    double cpu_usage_self_user;
    double cpu_usage_self_system;

    int64_t memory_usage_virtual;
    int64_t memory_usage_resident;
    int32_t num_threads;
};

void fetch_proc_stat(proc_stat_t* ostat);

void input_redirect(std::function<void(char)> inserter);
void input_rollback();
}  // namespace perfkit::terminal::net::detail
