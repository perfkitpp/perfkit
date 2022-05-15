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

#include "net.hpp"
#include "perfkit/configs.h"
#include "perfkit/terminal.h"

namespace perfkit::terminal::net {

struct profile;

perfkit::terminal_ptr create(profile const& cfg);
perfkit::terminal_ptr create(std::string config_name = "__NETCFG__");

PERFKIT_CFG_CLASS(profile)
{
    PERFKIT_T_CONFIGURE(session_name, "default")
            .env("PERFKIT_NET_SESSION_NAME");

    PERFKIT_T_CONFIGURE(session_description, "");

    PERFKIT_T_CONFIGURE(auth, "")
            .description("ID:PW:ACCESS;ID:PW:ACCESS;...")
            .env("PERFKIT_NET_AUTH");

    PERFKIT_T_CONFIGURE(bind_address, "0.0.0.0")
            .env("PERFKIT_NET_IPADDR");

    PERFKIT_T_CONFIGURE(bind_port, 0)
            .env("PERFKIT_NET_PORT");

    PERFKIT_T_CONFIGURE(enable_find_me, true);
};

}  // namespace perfkit::terminal::net