#pragma once
#include "net.hpp"

PERFKIT_CATEGORY(perfkit::terminal::net::flags)
{
    PERFKIT_CONFIGURE(name, "default")
            .transient()
            .hide()
            .flags("term-name")
            .confirm();

    PERFKIT_CONFIGURE(port, 0)
            .transient()
            .hide()
            .flags("term-port")
            .confirm();

    PERFKIT_CONFIGURE(auth, "")
            .transient()
            .hide()
            .flags("term-auth")
            .confirm();
}

namespace perfkit::terminal::net {
/**
 * Intentionally declared non-inline function, as this header must be included only for once. \n
 * Before calling this function, command line arguments must be parsed
 */
terminal_ptr create_from_flags()
{
    flags::update();

    terminal_init_info info{*flags::name};
    info.serve(*flags::port);
    info.parse_auth(*flags::auth);

    return create(info);
}

}  // namespace perfkit::terminal::net
