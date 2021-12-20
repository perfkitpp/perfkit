#include "perfkit/extension/net.hpp"

#include <perfkit/configs.h>

#include "net-terminal.hpp"

perfkit::terminal_ptr perfkit::terminal::net::create(const struct terminal_init_info& info)
{
    return std::make_shared<net::terminal>(info);
}

namespace perfkit::terminal::net::detail {
PERFKIT_T_CATEGORY(
        terminal_profile,

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

        PERFKIT_T_CONFIGURE(has_relay_server, 0)
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
}  // namespace perfkit::terminal::net::detail

perfkit::terminal_ptr perfkit::terminal::net::create(std::string config_profile_name)
{
    // TODO: parse auth info
    detail::terminal_profile cfg{std::move(config_profile_name)};
    cfg->update();

    terminal_init_info init{*cfg.session_name};
    auto CPPH_LOGGER = [] { return detail::nglog(); };

    CPPH_INFO("creating network session: {}", *cfg.session_name);

    // init.description; TODO: find description from description path
    // init.auth; TODO: parse auth

    for (std::string_view auth = *cfg.auth; not auth.empty();)
    {
        auto token = auth.substr(0, auth.find_first_of(';'));
        auth       = auth.substr(token.size() + (token.size() < auth.size()));

        if (token.empty())
            continue;

        try
        {
            auto id     = token.substr(0, token.find_first_of(':'));
            token       = token.substr(id.size() + 1);
            auto pw     = token.substr(0, token.find_first_of(':'));
            auto access = token.substr(pw.size() + 1);

            bool is_admin = access.find_first_of("wW");

            auto& info           = init.auths.emplace_back();
            info.id              = id;
            info.password        = pw;
            info.readonly_access = not is_admin;

            CPPH_INFO("adding {} auth {}", info.readonly_access ? "readonly" : "admin", info.id);
        }
        catch (std::out_of_range& ec)
        {
            glog()->error("auth format error: <ID>:<PW>:<ACCESS>[;<ID>:<PW>:<ACCESS>[;...]]");
        }
    }

    init.description = cfg.session_description.ref();
    *cfg.has_relay_server
            ? init.relay_to(*cfg.ipaddr, *cfg.port)
            : init.serve(*cfg.ipaddr, *cfg.port);
    CPPH_INFO("binding to {}:{}", *cfg.ipaddr, *cfg.port);

    return std::make_shared<net::terminal>(init);
}
