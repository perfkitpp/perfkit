// MIT License
//
// Copyright (c) 2021-2022. Seungwoo Kang
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// project home: https://github.com/perfkitpp

#pragma once
#include <list>
#include <tuple>

#include <nlohmann/json.hpp>
#include <cpph/helper/nlohmann_json_macros.hxx>

namespace perfkit::terminal::net::outgoing {
struct session_reset {
    constexpr static char ROUTE[] = "update:epoch";

    std::string           name;
    std::string           hostname;
    std::string           keystr;
    int64_t               epoch;
    std::string           description;
    int32_t               num_cores;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            session_reset, name, keystr, epoch, description, num_cores);
};

struct session_state {
    constexpr static char ROUTE[] = "update:session_state";

    double                cpu_usage_total_user;
    double                cpu_usage_total_system;
    double                cpu_usage_self_user;
    double                cpu_usage_self_system;

    int64_t               memory_usage_virtual;
    int64_t               memory_usage_resident;
    int32_t               num_threads;

    int32_t               bw_out;
    int32_t               bw_in;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            session_state,
            cpu_usage_total_user,
            cpu_usage_total_system,
            cpu_usage_self_user,
            cpu_usage_self_system,
            memory_usage_virtual,
            memory_usage_resident,
            num_threads,
            bw_out,
            bw_in);
};

struct shell_output {
    constexpr static char ROUTE[] = "update:shell_output";

    std::string           content;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            shell_output, content);
};

struct new_config_class {
    constexpr static char ROUTE[] = "update:new_config_class";

    struct entity_scheme {
        std::string    name;
        uint64_t       config_key;
        nlohmann::json value;
        nlohmann::json metadata;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                entity_scheme, name, config_key, value, metadata);
    };

    struct category_scheme {
        std::string                name;
        std::list<category_scheme> subcategories;
        std::list<entity_scheme>   entities;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                category_scheme, name, subcategories, entities);
    };

    std::string     key;
    category_scheme root;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            new_config_class, key, root);
};

struct config_entity {
    constexpr static char ROUTE[] = "update:config_entity";

    struct entity_scheme {
        uint64_t       config_key = {};
        nlohmann::json value;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                entity_scheme, config_key, value);
    };

    std::string                      class_key;
    std::forward_list<entity_scheme> content;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            config_entity, class_key, content);
};

struct suggest_command {
    constexpr static char          ROUTE[] = "rpc:suggest_command";

    int64_t                        reply_to;
    std::string                    new_command;
    std::forward_list<std::string> candidates;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            suggest_command, reply_to, new_command, candidates);
};

struct trace_class_list {
    constexpr static char ROUTE[] = "update:trace_class_list";

    // trace class name and its id
    std::forward_list<std::pair<std::string, uint64_t>> content;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            trace_class_list, content);
};

enum trace_value_type {
    TRACE_VALUE_NULLPTR,
    TRACE_VALUE_DURATION_USEC,
    TRACE_VALUE_INTEGER,
    TRACE_VALUE_FLOATING_POINT,
    TRACE_VALUE_STRING,
    TRACE_VALUE_BOOLEAN,
};

struct traces {
    constexpr static char ROUTE[] = "update:traces";

    struct node_scheme {
        std::string            name;
        uint64_t               trace_key;
        bool                   is_fresh;
        bool                   subscribing;
        bool                   folded;
        std::string            value;
        int                    value_type;
        std::list<node_scheme> children;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                node_scheme, name, trace_key, is_fresh,
                subscribing, folded, value, value_type, children);
    };

    std::string class_name;
    node_scheme root;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            traces, class_name, root);
};

}  // namespace perfkit::terminal::net::outgoing

namespace perfkit::terminal::net::incoming {
struct push_command {
    constexpr static char ROUTE[] = "cmd:push_command";

    std::string           command;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            push_command, command);
};

struct suggest_command {
    constexpr static char ROUTE[] = "rpc:suggest_command";

    int64_t               reply_to;
    int16_t               position;
    std::string           command;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            suggest_command, reply_to, position, command);
};

struct configure_entity {
    constexpr static char ROUTE[] = "cmd:configure";

    struct entity_scheme {
        uint64_t       config_key;
        nlohmann::json value;

        CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
                entity_scheme, config_key, value);
    };

    std::string              class_key;
    std::list<entity_scheme> content;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            configure_entity, class_key, content);
};

struct signal_fetch_traces {
    constexpr static char  ROUTE[] = "cmd:signal_fetch_traces";
    std::list<std::string> targets;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(signal_fetch_traces, targets);
};

struct control_trace {
    constexpr static char ROUTE[] = "cmd:control_trace";
    std::string           class_name;
    uint64_t              trace_key;
    std::optional<bool>   fold;
    std::optional<bool>   subscribe;

    CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(
            control_trace, class_name, trace_key, fold, subscribe);
};

}  // namespace perfkit::terminal::net::incoming
