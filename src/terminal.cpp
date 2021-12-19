//
// Created by Seungwoo on 2021-09-25.
//
#include "perfkit/terminal.h"

#include <filesystem>
#include <regex>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/subrange.hpp>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include "perfkit/common/format.hxx"
#include "perfkit/detail/base.hpp"
#include "perfkit/detail/commands.hpp"
#include "perfkit/detail/configs.hpp"
#include "perfkit/detail/trace_future.hpp"
#include "perfkit/detail/tracer.hpp"

using namespace std::literals;

namespace perfkit::terminal {
using commands::args_view;
using commands::command_already_exist_exception;
using commands::command_name_invalid_exception;
using commands::string_set;

class _config_saveload_manager
{
   public:
    bool load_from(args_view args = {})
    {
        auto path = args.empty() ? _latest : args.front();
        _latest   = path;
        return perfkit::configs::import_file(path);
    }

    bool save_to(args_view args = {})
    {
        auto path = args.empty() ? _latest : args.front();
        _latest   = path;
        return perfkit::configs::export_to(path);
    }

    void retrieve_filenames(args_view args, string_set& cands)
    {
        namespace fs = std::filesystem;

        fs::path path;
        if (not args.empty()) { path = args[0]; }
        if (path.empty()) { path = "./"; }

        if (not is_directory(path)) { path = path.parent_path(); }
        if (path.empty()) { path = "./"; }

        fs::directory_iterator it{path}, end{};
        std::transform(
                it, end, std::inserter(cands, cands.end()),
                [](auto&& p) {
                    fs::path path = p.path();
                    auto&& str    = path.string();

                    return fs::is_directory(path) ? str.append("/") : str;
                });
    }

   public:
    std::string _latest = {};
};

void register_conffile_io_commands(
        perfkit::if_terminal* ref,
        std::string_view cmd_load,
        std::string_view cmd_store,
        std::string_view initial_path)
{
    auto manip     = std::make_shared<_config_saveload_manager>();
    manip->_latest = initial_path;

    auto rootnode  = ref->commands()->root();
    auto node_load = rootnode->add_subcommand(
            std::string{cmd_load},
            [manip](auto&& tok) { return manip->load_from(tok); },
            [manip](auto&& tok, auto&& set) { return manip->retrieve_filenames(tok, set); });

    auto node_save = rootnode->add_subcommand(
            std::string{cmd_store},
            [manip](auto&& tok) { return manip->save_to(tok); },
            [manip](auto&& tok, auto&& set) { return manip->retrieve_filenames(tok, set); });

    if (!node_load || !node_save) { throw command_already_exist_exception{}; }
}

void register_logging_manip_command(if_terminal* ref, std::string_view cmd)
{
    std::string cmdstr{cmd};

    struct fn_op
    {
        if_terminal* ref;
        std::string logger_name;

        bool operator()(args_view args)
        {
            spdlog::logger* logger = {};
            if (logger_name.empty())
            {
                logger = spdlog::default_logger_raw();
            }
            else
            {
                logger = spdlog::get(logger_name).get();
            }

            if (!logger)
            {
                SPDLOG_LOGGER_ERROR(glog(), "dead logger '{}'", logger_name);
                return false;
            }

            if (args.empty())
            {
                ref->write("{} = {}\n"_fmt
                           % logger_name
                           % spdlog::level::to_string_view(logger->level())
                           / 20);
            }
            else if (args.size() == 1)
            {
                logger->set_level(spdlog::level::from_str(std::string{args[0]}));
            }
            else
            {
                return false;
            }
            return true;
        }
    };

    auto fn_sugg = [](auto&&, string_set& s) {
        s.insert({"trace", "debug", "info", "warn", "error", "critical"});
    };

    auto logging = ref->commands()->root()->add_subcommand(cmdstr);
    logging->reset_opreation_hook(
            [ref, cmdstr, fn_sugg](commands::registry::node* node, auto&&) {
                spdlog::details::registry::instance().apply_all(
                        [&](const std::shared_ptr<spdlog::logger>& logger) {
                            if (logger->name().empty()) { return; }
                            if (node->is_valid_command(logger->name())) { return; }
                            node->add_subcommand(logger->name(), fn_op{ref, logger->name()}, fn_sugg);
                        });
            });

    logging->add_subcommand("_default_", fn_op{ref, ""}, fn_sugg);
    logging->add_subcommand(
            "_global_",
            [ref](args_view args) -> bool {
                if (args.empty())
                {
                    std::string buf;
                    spdlog::details::registry::instance().apply_all(
                            [&](const std::shared_ptr<spdlog::logger>& logger) {
                                auto name = logger->name();
                                if (name.empty()) { name = "_default_"; }

                                ref->write(buf < ("{} = {}\n"_fmt
                                                  % name
                                                  % spdlog::level::to_string_view(logger->level())));
                            });
                }
                else if (args.size() == 1)
                {
                    spdlog::set_level(spdlog::level::from_str(std::string{args[0]}));
                }
                else
                {
                    return false;
                }
                return true;
            },
            fn_sugg);
}

class _trace_manip
{
   public:
    _trace_manip(if_terminal* ref, commands::registry::node* root)
    {
        _ref = ref;
        SPDLOG_LOGGER_TRACE(glog(), "REF PTR: {}", (void*)ref);
        root->reset_suggest_handler(
                [this](auto&&, auto&& s) { suggest(s); });
    }

    void help() const
    {
        _ref->write("usage: <cmd> <tracer> [<regex filter> [true|false]]\n");
    }

    void suggest(string_set& repos)
    {
        // list of tracers
        for (auto const& tracer : tracer::all())
        {
            repos.insert(tracer->name());
        }
    }

    bool invoke(args_view args)
    {
        if (args.empty() || args.size() > 3) { return help(), false; }

        if (_async.valid())
        {
            auto r_wait = _async.wait_for(200ms);

            if (r_wait == std::future_status::timeout)
            {
                SPDLOG_LOGGER_WARN(glog(), "previous trace lookup request is under progress ...");
                return false;
            }
            else if (r_wait == std::future_status::ready)
            {
                try
                {
                    _async.get();
                }
                catch (std::regex_error& e)
                {
                    SPDLOG_LOGGER_WARN(glog(), "regex error on trace filter: {}", e.what());
                }
            }
            else
            {
                throw std::logic_error("async operation must not be deffered.");
            }
        }

        std::string pattern{".*"};
        std::optional<bool> setter;

        if (args.size() > 1) { pattern.assign(args[1].begin(), args[1].end()); }
        if (args.size() > 2)
        {
            args[2] == "true" && (setter = true) || args[2] == "false" && (setter = false);
        }

        using namespace ranges;
        auto traces = tracer::all();

        auto it = std::find_if(traces.begin(),
                               traces.end(),
                               [&](auto& p) {
                                   return p->name() == args[0];
                               });

        if (it == traces.end())
        {
            SPDLOG_LOGGER_ERROR(glog(), "name '{}' is not valid tracer name", args[0]);
            return false;
        }

        auto trc = *it;
        _async   = std::async(std::launch::async, [=] { _async_request(trc, pattern, setter); });
        return true;
    }

   private:
    void _async_request(std::shared_ptr<tracer> ref, std::string pattern, std::optional<bool> setter)
    {
        tracer::future_result fut;
        ref->async_fetch_request(&fut);

        if (fut.valid() == false)
        {
            SPDLOG_LOGGER_ERROR(glog(), "tracer '{}' returned invalid fetch request result.", ref->name());
            return;
        }

        if (fut.wait_for(3s) == std::future_status::timeout)
        {
            SPDLOG_LOGGER_ERROR(glog(), "tracer '{}' update timeout", ref->name());
            return;
        }

        tracer::fetched_traces result;
        fut.get().copy_sorted(result);

        using namespace ranges;
        std::regex match{pattern};
        std::string output;
        output << "\n"_fmt.s();

        array_view<std::string_view> current_hierarchy = {};
        std::string hierarchy_key, data_str, full_key;
        for (auto& item : result)
        {
            auto hierarchy = item.hierarchy.subspan(0, item.hierarchy.size() - 1);

            full_key.clear();
            for (auto c : item.hierarchy | views::join(".")) { full_key += c; }

            if (not std::regex_match(full_key, match)) { continue; }
            if (setter) { item.subscribe(*setter); }

            bool hierarchy_changed = hierarchy != current_hierarchy;
            if (hierarchy_changed)
            {
                current_hierarchy = hierarchy;
                hierarchy_key.clear();
                for (auto c : hierarchy | views::join(".")) { hierarchy_key += c; }
                if (not hierarchy_key.empty()) { hierarchy_key += '.'; }
            }

            // append string
            if (hierarchy_changed)
            {
                output << "{}{}"_fmt % hierarchy_key % item.key;
            }
            else
            {
                output << "{:{}}{}"_fmt % "" % hierarchy_key.size() % item.key;
            }

            item.dump_data(data_str);
            output << "{}= {}\n"_fmt % (item.subscribing() ? "(+) " : " ") % data_str;
        }

        _ref->write(output);
    }

   private:
    if_terminal* _ref;
    std::future<void> _async;
};

void register_trace_manip_command(if_terminal* ref, std::string_view cmd)
{
    auto node = ref->commands()->root()->add_subcommand(std::string{cmd});
    node->reset_invoke_handler(
            [ptr = std::make_shared<_trace_manip>(ref, node)]  //
            (auto&& args) {
                return ptr->invoke(args);
            });
}

void initialize_with_basic_commands(if_terminal* ref)
{
    register_logging_manip_command(ref);
    register_trace_manip_command(ref);
    register_conffile_io_commands(ref);
    register_config_manip_command(ref);
}

void register_config_manip_command(if_terminal* ref, std::string_view cmd)
{
    auto _locked  = ref->commands()->root()->acquire();
    auto node_cmd = ref->commands()->root()->add_subcommand(std::string{cmd});

    auto node_set = node_cmd->add_subcommand("set");
    auto node_get = node_cmd->add_subcommand("get");

    using node_type = commands::registry::node;
    auto hook_enum_regs =
            [](auto&& hook_factory) {
                return [=](node_type* node_reg, auto&&) {
                    glog()->debug("registry hook: {}", (void*)node_reg);

                    auto _ = node_reg->acquire();
                    node_reg->clear();

                    for (const auto& registry : config_registry::bk_enumerate_registries())
                    {
                        auto node = node_reg->add_subcommand(registry->name());
                        node->reset_opreation_hook(hook_factory(registry));
                    }
                };
            };

    node_set->reset_opreation_hook(
            hook_enum_regs([](std::weak_ptr<config_registry> wrg) {
                return [wrg](node_type* node_cfg, auto&&) {
                    glog()->debug("config hook: {}", (void*)node_cfg);

                    auto rg = wrg.lock();
                    if (not rg) { return; }

                    for (const auto& [_, config] : rg->bk_all())
                    {
                        node_cfg->add_subcommand(
                                config->display_key(),
                                [wconf = std::weak_ptr{config}](args_view args) {
                                    if (args.empty())
                                    {
                                        glog()->error("command 'set' requires argument");
                                        return;
                                    }

                                    auto conf = wconf.lock();
                                    if (not conf) { return; }

                                    auto value  = args[0];
                                    auto parsed = json::parse(value.begin(), value.end(), nullptr, false);

                                    if (parsed.is_discarded())
                                    {
                                        glog()->error("failed to parse input value");
                                        return;
                                    }

                                    conf->request_modify(std::move(parsed));
                                });
                    }
                };
            }));

    node_get->reset_opreation_hook(
            hook_enum_regs([ref](std::weak_ptr<config_registry> wrg) {
                return [wrg, ref](node_type* node_cfg, auto&&) {
                    if (auto rg = wrg.lock())
                    {
                        // register config info command
                        auto all = rg->bk_all();

                        for (auto& [key, config] : all)
                        {
                            node_cfg->add_subcommand(
                                    config->display_key(),
                                    [ref, _conf = config] {
                                        std::string buf{};
                                        buf.reserve(1024);

                                        buf.append("\n");
                                        buf << "<name        > {}\n"_fmt % _conf->display_key();
                                        buf << "<key         > {}\n"_fmt % _conf->full_key();
                                        buf << "<description > {}\n"_fmt % _conf->description();
                                        buf << "<attributes  > {}\n"_fmt % _conf->attribute().dump(2);
                                        buf << "<value       > {}\n"_fmt % _conf->serialize().dump(2);

                                        ref->write(buf);
                                    });
                        }
                    }
                    else
                    {
                        return;
                    }

                    node_cfg->reset_invoke_handler(
                            [wrg, ref](args_view args) {
                                // list all configurations belong to this category.
                                using namespace ranges;
                                auto rg = wrg.lock();
                                if (not rg) { return false; }

                                auto all = &rg->bk_all();

                                // use first token as pattern
                                std::string_view _category = "";
                                if (not args.empty()) { _category = args[0]; }

                                std::string buf;
                                buf.reserve(1024);

                                // header
                                buf << "< {} >\n"_fmt % (_category);

                                // during string begins with this category name ...
                                for (const auto& [_, conf] : *all)
                                {
                                    if (conf->display_key().find(_category) != 0) { continue; }

                                    // auto depth = conf->tokenized_display_key().size();
                                    auto name = conf->display_key();  //.substr(_category.size());

                                    buf << " {} = {}\n"_fmt % name % conf->serialize().dump();
                                }

                                buf += "\n";
                                ref->write(buf);
                                return true;
                            });
                };
            }));
}

}  // namespace perfkit::terminal
