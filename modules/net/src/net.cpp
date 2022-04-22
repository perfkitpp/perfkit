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

#include "perfkit/extension/net.hpp"

#include <cpph/algorithm/base64.hxx>
#include <perfkit/configs.h>
#include <perfkit/extension/netconf.h>

#include "net-terminal.hpp"
#include "picosha2.h"

perfkit::terminal_ptr perfkit::terminal::net::create(const struct terminal_init_info& info)
{
    return std::make_shared<net::terminal>(info);
}

namespace perfkit::terminal::net {
static void parse_auth(std::string_view auth, std::vector<auth_info>* out)
{
    auto CPPH_LOGGER = [] { return detail::nglog(); };

    for (; not auth.empty();) {
        auto token = auth.substr(0, auth.find_first_of(';'));
        auth = auth.substr(token.size() + (token.size() < auth.size()));

        if (token.empty())
            continue;

        try {
            auto id = token.substr(0, token.find_first_of(':'));
            token = token.substr(id.size() + 1);
            auto  pw = token.substr(0, token.find_first_of(':'));
            auto  access = token.substr(pw.size() + 1);

            bool  is_admin = access.find_first_of("wW") == 0;

            auto& info = out->emplace_back();
            info.id = id;
            info.readonly_access = not is_admin;

            // parse password as sha256
            std::array<char, 32> array;
            picosha2::hash256(pw, array);

            std::array<char, perfkit::base64::encoded_size(sizeof(array))> b64hash;
            perfkit::base64::encode(array, b64hash.begin());

            info.password.assign(b64hash.begin(), b64hash.end());
            CPPH_INFO("adding {} auth {}", info.readonly_access ? "readonly" : "admin", info.id);
        } catch (std::out_of_range& ec) {
            glog()->error("auth format error: <ID>:<PW>:<ACCESS>[;<ID>:<PW>:<ACCESS>[;...]]");
        }
    }
}
}  // namespace perfkit::terminal::net

perfkit::terminal_ptr perfkit::terminal::net::create(profile const& cfg)
{
    terminal_init_info init{*cfg.session_name};
    auto               CPPH_LOGGER = [] { return detail::nglog(); };

    CPPH_INFO("creating network session: {}", *cfg.session_name);
    init.description = cfg.session_description.ref();

    if (*cfg.has_relay_server) {
        parse_auth(cfg.auth.ref(), &init.auths);

        init.relay_to(*cfg.ipaddr, *cfg.port);
        CPPH_INFO("connecting to relay server {}:{}", *cfg.ipaddr, *cfg.port);
    } else {
        init.serve(*cfg.ipaddr, *cfg.port);
        CPPH_INFO("server binding to {}:{}", *cfg.ipaddr, *cfg.port);
    }

    return std::make_shared<net::terminal>(init);
}

perfkit::terminal_ptr perfkit::terminal::net::create(std::string config_profile_name)
{
    profile cfg{std::move(config_profile_name)};
    cfg->update();
    return create(cfg);
}

void perfkit::terminal::net::terminal_init_info::parse_auth(std::string_view authstr)
{
    net::parse_auth(authstr, &auths);
}
