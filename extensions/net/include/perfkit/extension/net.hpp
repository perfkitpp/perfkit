/*******************************************************************************
 * MIT License
 *
 * Copyright (c) 2021. Seungwoo Kang
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

#pragma once
#include <vector>

#include "perfkit/terminal.h"

namespace perfkit::terminal::net {
/**
 * Create net client session instance
 *
 * @param info
 */
terminal_ptr create(struct terminal_init_info const& info);

/**
 * Create net client from configuration profile
 *
 * @param config_profile_name
 *
 * @details
 *      You can configure terminal initialization parameters by following environment variables
 *      - PERFKIT_NET_SESSION_NAME
 *      - PERFKIT_NET_AUTH
 *          <ID>:<PW>:<ACCESS>[;<ID>:<PW>:<ACCESS>[;...]]
 *      - PERFKIT_NET_USE_RELAY_SERVER
 *      - PERFKIT_NET_IPADDR
 *      - PERFKIT_NET_PORT
 */
terminal_ptr create(std::string config_profile_name = "__NETTERM%%");

enum class operation_mode
{
    invalid,
    independent_server,
    relay_server_provider,
};

struct auth_info
{
    std::string id;
    std::string password;

    // If readonly access is enabled, every command without 'auth:login' will be discarded.
    bool readonly_access = true;
};

struct terminal_init_info
{
    /** Session name to introduce self */
    std::string const name;

    /** Describes this session */
    std::string description;

    /**
     * Authentication information. If empty, every login attempt will be permitted. */
    std::vector<auth_info> auths;

    /**
     * Advanced configurations
     */
    struct _adv_t
    {
        bool redirect_terminal = true;
    } advanced;

    /**
     * Create terminal instance as an independent server.
     * Multiple clients will directly connect to this instance, which may cause
     *  performance problem when they're too many.
     *
     * @param bindaddr
     * @param port
     */
    void serve(std::string bindaddr, uint16_t port)
    {
        _mode         = operation_mode::independent_server;
        _string_param = std::move(bindaddr);
        _port_param   = port;
    }

    void serve(uint16_t port) { serve("0.0.0.0", port); }

    /**
     * Create terminal instance as an relay-server provider.
     * Metadata of this session will be registered to relay server, and only one tcp socket
     *  connection is maintained while up. Clients will indirectly communicate with this
     *  net instance through relay server.
     */
    void relay_to(std::string addr, uint16_t port)
    {
        _mode = operation_mode::relay_server_provider;
        // TODO.
    }

    /**
     * Generate auth info from given auth string.
     * Auth string should be form of <ID>:<PW>:<ACCESS>[;<ID>:<PW>:<ACCESS>[;...]]
     *
     * @param authstr
     */
    void parse_auth(std::string_view authstr) {}

   private:
    friend class dispatcher;  // internal use
    operation_mode _mode = operation_mode::invalid;
    std::string _string_param;
    uint16_t _port_param = 0;

   public:
    explicit terminal_init_info(std::string session_name) : name{std::move(session_name)} {};
};

}  // namespace perfkit::terminal::net
