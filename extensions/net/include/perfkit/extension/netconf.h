#pragma once

#include "net.hpp"
#include "perfkit/configs.h"

namespace perfkit::terminal::net {

struct profile;

perfkit::terminal_ptr create(profile const& cfg);

PERFKIT_T_CATEGORY(
        profile,

        PERFKIT_T_CONFIGURE(session_name, "default")
                .env("PERFKIT_NET_SESSION_NAME")
                .readonly()
                .confirm();

        PERFKIT_T_CONFIGURE(session_description, "")
                .confirm();

        PERFKIT_T_CONFIGURE(auth, "")
                .description("ID:PW:ACCESS;ID:PW:ACCESS;...")
                .readonly()
                .env("PERFKIT_NET_AUTH")
                .confirm();

        PERFKIT_T_CONFIGURE(has_relay_server, false)
                .env("PERFKIT_NET_USE_RELAY_SERVER")
                .readonly()
                .confirm();

        PERFKIT_T_CONFIGURE(ipaddr, "0.0.0.0")
                .env("PERFKIT_NET_IPADDR")
                .readonly()
                .confirm();

        PERFKIT_T_CONFIGURE(port, 15572)
                .env("PERFKIT_NET_PORT")
                .readonly()
                .confirm();

);

}  // namespace perfkit::terminal::net