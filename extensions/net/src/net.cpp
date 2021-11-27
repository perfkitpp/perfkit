#include "perfkit/extension/net.hpp"

#include <perfkit/configs.h>

#include "net-terminal.hpp"

perfkit::terminal_ptr perfkit::terminal::net::create(const struct terminal_init_info& info)
{
    return std::make_shared<net::terminal>(info);
}

namespace perfkit::terminal::net::detail
{
PERFKIT_T_CATEGORY(
        terminal_profile,

        PERFKIT_T_CONFIGURE(session_name, "default")
                .env("PERFKIT_NET_SESSION_NAME")
                .confirm();

        PERFKIT_T_CONFIGURE(session_description_path, "")
                .env("PERFKIT_NET_SESSION_DESCRIPTION_PATH")
                .confirm();

        PERFKIT_T_CONFIGURE(auth, "")
                .description("ID:PW:ACCESS;ID:PW:ACCESS;...")
                .env("PERFKIT_NET_AUTH")
                .confirm();

        PERFKIT_T_CONFIGURE(has_relay_server, 0)
                .env("PERFKIT_NET_USE_RELAY_SERVER")
                .confirm();

        PERFKIT_T_CONFIGURE(ipaddr, "0.0.0.0")
                .env("PERFKIT_NET_IPADDR")
                .confirm();

        PERFKIT_T_CONFIGURE(port, 0)
                .env("PERFKIT_NET_PORT")
                .confirm();

);
}  // namespace perfkit::terminal::net::detail

perfkit::terminal_ptr perfkit::terminal::net::create(std::string config_profile_name)
{
    // TODO: parse auth info
    detail::terminal_profile cfg{std::move(config_profile_name)};
    cfg->update();

    terminal_init_info init{*cfg.session_name};
    // init.description; TODO: find description from description path
    // init.auth; TODO: parse auth

    *cfg.has_relay_server ? init.relay_to(*cfg.ipaddr, *cfg.port)
                          : init.serve(*cfg.ipaddr, *cfg.port);

    return std::make_shared<net::terminal>(init);
}
