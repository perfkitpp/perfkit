#pragma once
#include <nlohmann/json.hpp>
#include <perfkit/common/helper/nlohmann_json_macros.hxx>

namespace perfkit::terminal::net::outgoing
{
struct session_reset
{
    std::string name;
    std::string keystr;
    int64_t epoch;
    std::string description;
    int32_t num_cores;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            session_reset, name, keystr, epoch, description, num_cores);
};

struct session_state
{
    double cpu_usage;
    int64_t memory_usage;
    int32_t bw_out;
    int32_t bw_in;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            session_state, cpu_usage, memory_usage, bw_out, bw_in);
};

struct shell_output
{
    std::string content;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            shell_output, content);
};
}  // namespace perfkit::terminal::net::outgoing

namespace perfkit::terminal::net::incoming
{
struct push_command
{
    std::string command;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            push_command, command);
};
}  // namespace perfkit::terminal::net::incoming
