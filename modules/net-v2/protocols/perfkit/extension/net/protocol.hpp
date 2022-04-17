/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2022. Seungwoo Kang
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * project home: https://github.com/perfkitpp
 ******************************************************************************/

//
// Created by ki608 on 2022-03-06.
//

#pragma once
#include <chrono>
#include <list>
#include <variant>

#include "perfkit/common/refl/core.hxx"
#include "perfkit/common/refl/rpc/signature.hxx"

namespace perfkit::msgpack::rpc {
class context;
}

namespace perfkit::net::message {
using std::list;
using std::string;
using std::vector;

using std::chrono::microseconds;
using std::chrono::milliseconds;

using msgpack_archive_t = binary<string>;

#define DEFINE_RPC(Name, ...) \
    static inline const auto Name = rpc::create_signature<__VA_ARGS__>(#Name);

/**
 * Shell output descriptor
 */
struct tty_output_t {
    CPPH_REFL_DECLARE_c;

    int64_t fence = 0;
    string  content;
};

/**
 * Configuration descriptor
 */
struct config_entity_t {
    CPPH_REFL_DECLARE_c;

    string            name;
    uint64_t          config_key;

    string            description;
    msgpack_archive_t initial_value;

    msgpack_archive_t opt_min;
    msgpack_archive_t opt_max;
    msgpack_archive_t opt_one_of;
};

struct config_entity_update_t {
    CPPH_REFL_DECLARE_c;

    uint64_t          config_key;
    msgpack_archive_t content_next;  // msgpack data chunk for supporting 'any'
};

enum class auth_level_t {
    unauthorized,
    basic_access,
    admin_access
};

/**
 * Trace descriptor
 */
struct tracer_descriptor_t {
    CPPH_REFL_DECLARE_c;

    uint64_t     tracer_id;
    string       name;
    int          priority;
    milliseconds epoch;
};

using trace_payload_t = std::variant<nullptr_t, microseconds, int64_t, double, string, bool>;

struct trace_info_t {
    CPPH_REFL_DECLARE_c;

    int      index;  // Unique occurrence order. As nodes never expires, this value can simply be used as index.

    int      parent_index;     // Birth index of parent node. Required to build hierarchy. -1 if root.
    string   name;
    uint64_t hash;             // Unique hash
    uint64_t owner_tracer_id;  // Owning tracer's id
};

struct trace_update_t {
    CPPH_REFL_DECLARE_c;

    uint64_t        index;  // To lookup corresponding information ...

    int64_t         fence_value;
    int             occurrence_order;

    bool            flags[2];
    trace_payload_t payload;

   public:
    auto& ref_subscr() { return flags[0]; }
    auto& ref_fold() { return flags[1]; }
};

//
// SHELL
//
struct notify {
    /**
     * Returns current session status
     */
    struct session_status_t {
        CPPH_REFL_DECLARE_c;

        double  cpu_usage_total_user;
        double  cpu_usage_total_system;
        double  cpu_usage_self_user;
        double  cpu_usage_self_system;

        int64_t memory_usage_virtual;
        int64_t memory_usage_resident;
        int32_t num_threads;

        int32_t bw_out;
        int32_t bw_in;
    };

    DEFINE_RPC(session_status, void(session_status_t));

    /**
     * Shell content output on every flush
     */
    DEFINE_RPC(tty, void(tty_output_t));

    /**
     * Configuration update notifies
     */
    DEFINE_RPC(config_entity_update, void(config_entity_update_t));

    /**
     * Configuration class notifies
     */
    struct config_category_t {
        CPPH_REFL_DECLARE_c;

        string                  name;
        list<config_category_t> subcategories;
        list<config_entity_t>   entities;
    };

    DEFINE_RPC(new_config_category, void(uint64_t instance_hash, string registry_key, config_category_t root));
    DEFINE_RPC(deleted_config_category, void(string registry_key));

    /**
     * Trace update notification
     */
    DEFINE_RPC(new_tracer, void(tracer_descriptor_t));
    DEFINE_RPC(deleted_tracer, void(uint64_t tracer_id));
    DEFINE_RPC(new_trace_node, void(uint64_t tracer_id, vector<trace_info_t>));
    DEFINE_RPC(trace_node_update, void(uint64_t tracer_id, vector<trace_update_t>));

    /**
     * Graphic class changes
     */

    /**
     *
     */
};

struct service {
    /**
     * Query session informations
     */
    struct session_info_t {
        CPPH_REFL_DECLARE_c;

        string  name;
        string  hostname;
        string  keystr;
        int64_t epoch;
        string  description;
        int32_t num_cores;
    };

    DEFINE_RPC(session_info, session_info_t());

    /**
     * Query if server is alive
     */
    DEFINE_RPC(heartbeat, void(void));

    /**
     * Requests tty output from since given fence value
     */
    DEFINE_RPC(fetch_tty, tty_output_t(int64_t fence));

    /**
     * Authentications
     */
    DEFINE_RPC(login, auth_level_t(string));

    /**
     * Invoke command
     */
    DEFINE_RPC(invoke_command, void(string));

    /**
     * Request republish all registry lists.
     */
    DEFINE_RPC(request_republish_registries, void());

    /**
     * Request fetch trace from given fence.
     *
     * Set occurrence_index -1
     */
    DEFINE_RPC(trace_request_update, void(uint64_t tracer_id));

    /**
     * Reset transfer cache levels.
     *
     * After this function call, all trace node will be transferred again from next notification.
     */
    DEFINE_RPC(trace_reset_cache, void(uint64_t tracer_id));

    /**
     * Request trace state manipulation
     */
    struct trace_control_t {
        CPPH_REFL_DECLARE_c;

        std::array<bool, 2> flags;

        auto&               ref_subscr() { return flags[0]; }
        auto&               ref_fold() { return flags[1]; }
    };

    DEFINE_RPC(trace_request_control, void(uint64_t tracer_id, int index, trace_control_t));

    /**
     * Command suggest
     */
    struct suggest_result_t {
        CPPH_REFL_DECLARE_c;

        std::pair<int, int> replace_range;
        string              replaced_content;
        vector<string>      candidate_words;
    };

    DEFINE_RPC(suggest, suggest_result_t(string command, int cursor));

    /**
     * Update config
     */
    DEFINE_RPC(update_config_entity, void(config_entity_update_t));
};

#undef DEFINE_RPC
}  // namespace perfkit::net::message
