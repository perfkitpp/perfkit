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
#include <list>

#include "perfkit/common/refl/core.hxx"
#include "perfkit/common/refl/msgpack-rpc/signature.hxx"

namespace perfkit::msgpack::rpc {
class context;
}

namespace perfkit::net::message {
using std::list;
using std::string;
using std::vector;

using msgpack_archive_t = binary<string>;
using token_t           = string;

#define DEFINE_IFACE(Name, ...) \
    static inline const auto Name = msgpack::rpc::create_signature<__VA_ARGS__>(#Name);

/**
 * Shell output descriptor
 */
struct tty_output_t
{
    CPPH_REFL_DECLARE_c;

    int64_t fence = 0;
    string  content;
};

/**
 * Configuration descriptor
 */
struct config_entity_t
{
    CPPH_REFL_DECLARE_c;

    string            name;
    uint64_t          config_key;

    string            description;

    msgpack_archive_t opt_min;
    msgpack_archive_t opt_max;
    msgpack_archive_t opt_one_of;
};

struct config_category_t
{
    CPPH_REFL_DECLARE_c;

    string                       name;
    std::list<config_category_t> subcategories;
    std::list<config_entity_t>   entities;
};

//
// SHELL
//
struct notify
{
    /**
     * Shell content output on every flush
     */
    DEFINE_IFACE(tty, void(tty_output_t));

    /**
     * Periodically publishes session state
     */
    struct session_state_t
    {
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

    DEFINE_IFACE(session_state, void(session_state_t));

    /**
     * Notification on configuration class creation
     */

    /**
     * Configuration update notifies
     */
    struct config_update_t
    {
        CPPH_REFL_DECLARE_c;

        uint64_t          config_key;
        msgpack_archive_t content_next;  // msgpack data chunk for supporting 'any'
    };

    DEFINE_IFACE(config_update, vector<config_update_t>(void));
};

struct service
{
    /**
     * Query if server is alive
     */
    DEFINE_IFACE(heartbeat, void(void));

    /**
     * Requests tty output from since given fence value
     */
    DEFINE_IFACE(fetch_tty, tty_output_t(int64_t fence));

    /**
     * Authentications
     */
    DEFINE_IFACE(login, token_t(string));
    DEFINE_IFACE(refresh_session, token_t(token_t));

    /**
     * Invoke command
     */
    DEFINE_IFACE(invoke_command, void(token_t, string));

    /**
     * Command suggest
     */
    struct suggest_result_t
    {
        CPPH_REFL_DECLARE_c;

        std::pair<int, int> replace_range;
        string              replaced_content;
        vector<string>      candidate_words;
    };

    DEFINE_IFACE(suggest, suggest_result_t(string command, int cursor));
};

#undef DEFINE_IFACE
}  // namespace perfkit::net::message
