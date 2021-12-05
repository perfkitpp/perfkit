#pragma once
#include <list>

#include <nlohmann/json.hpp>
#include <perfkit/common/helper/nlohmann_json_macros.hxx>

namespace perfkit::terminal::net::outgoing {
struct session_reset
{
    constexpr static char ROUTE[] = "update:epoch";

    std::string name;
    std::string hostname;
    std::string keystr;
    int64_t epoch;
    std::string description;
    int32_t num_cores;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            session_reset, name, keystr, epoch, description, num_cores);
};

struct session_state
{
    constexpr static char ROUTE[] = "update:session_state";

    double cpu_usage;
    int64_t memory_usage;
    int32_t bw_out;
    int32_t bw_in;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            session_state, cpu_usage, memory_usage, bw_out, bw_in);
};

struct shell_output
{
    constexpr static char ROUTE[] = "update:shell_output";

    std::string content;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            shell_output, content);
};

struct new_config_class
{
    constexpr static char ROUTE[] = "update:new_config_class";

    struct entity_scheme
    {
        std::string name;
        uint64_t config_key;
        nlohmann::json value;
        nlohmann::json metadata;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                entity_scheme, name, config_key, value, metadata);
    };

    struct category_scheme
    {
        std::string name;
        std::list<category_scheme> subcategories;
        std::list<entity_scheme> entities;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                category_scheme, name, subcategories, entities);
    };

    std::string key;
    category_scheme root;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            new_config_class, key, root);
};

struct config_entity
{
    constexpr static char ROUTE[] = "update:config_entity";

    struct entity_scheme
    {
        uint64_t config_key;
        nlohmann::json value;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                entity_scheme, config_key, value);
    };

    std::string class_key;
    std::forward_list<entity_scheme> content;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            config_entity, class_key, content);
};

struct suggest_command
{
    constexpr static char ROUTE[] = "rpc:suggest_command";

    int64_t reply_to;
    std::string new_command;
    std::forward_list<std::string> candidates;
};

}  // namespace perfkit::terminal::net::outgoing

namespace perfkit::terminal::net::incoming {
struct push_command
{
    constexpr static char ROUTE[] = "cmd:push_command";

    std::string command;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            push_command, command);
};

struct suggest_command
{
    constexpr static char ROUTE[] = "rpc:suggest_command";

    int64_t reply_to;
    int16_t position;
    std::string command;
};
}  // namespace perfkit::terminal::net::incoming
