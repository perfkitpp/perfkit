#include "perfkit/extension/net.hpp"

#include <perfkit/configs.h>
#include <perfkit/extension/netconf.h>

#include "net-terminal.hpp"

perfkit::terminal_ptr perfkit::terminal::net::create(const struct terminal_init_info& info)
{
    return std::make_shared<net::terminal>(info);
}

perfkit::terminal_ptr perfkit::terminal::net::create(profile const& cfg)
{
    terminal_init_info init{*cfg.session_name};
    auto CPPH_LOGGER = [] { return detail::nglog(); };

    CPPH_INFO("creating network session: {}", *cfg.session_name);

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

    if (*cfg.has_relay_server)
    {
        init.relay_to(*cfg.ipaddr, *cfg.port);
        CPPH_INFO("connecting to relay server {}:{}", *cfg.ipaddr, *cfg.port);
    }
    else
    {
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
