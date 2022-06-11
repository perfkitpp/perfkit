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

//
// Created by Seungwoo on 2021-09-25.
//
#include "perfkit/terminal.h"

#include <filesystem>
#include <future>
#include <regex>

#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/subrange.hpp>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include "cpph/utility/format.hxx"
#include "perfkit/detail/base.hpp"
#include "perfkit/detail/commands.hpp"
#include "perfkit/detail/configs-v2.hpp"
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
        _latest = path;
        return v2::configs_import(path);
    }

    bool save_to(args_view args = {})
    {
        auto path = args.empty() ? _latest : args.front();
        _latest = path;
        return v2::configs_export(path);
    }

    static void retrieve_filenames(args_view args, string_set& cands)
    {
        namespace fs = std::filesystem;

        fs::path path;
        if (not args.empty()) { path = args[0]; }
        if (path.empty()) { path = "./"; }

        if (not is_directory(path)) { path = path.parent_path(); }
        if (path.empty()) { path = "./"; }

        fs::directory_iterator it{fs::is_directory(path) ? path : path.parent_path()}, end{};
        std::transform(
                it, end, std::inserter(cands, cands.end()),
                [](auto&& p) {
                    fs::path path = p.path();
                    auto&& str = path.string();
                    std::replace(str.begin(), str.end(), '\\', '/');

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
    auto manip = std::make_shared<_config_saveload_manager>();
    manip->_latest = initial_path;

    auto rootnode = ref->commands()->root();
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

    struct fn_op {
        if_terminal* ref;
        std::string logger_name;

        bool operator()(args_view args)
        {
            spdlog::logger* logger = {};
            if (logger_name.empty()) {
                logger = spdlog::default_logger_raw();
            } else {
                logger = spdlog::get(logger_name).get();
            }

            if (!logger) {
                SPDLOG_LOGGER_ERROR(glog(), "dead logger '{}'", logger_name);
                return false;
            }

            if (args.empty()) {
                ref->write("{} = {}\n"_fmt
                           % logger_name
                           % spdlog::level::to_string_view(logger->level())
                           / 20);
            } else if (args.size() == 1) {
                logger->set_level(spdlog::level::from_str(std::string{args[0]}));
            } else {
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
                if (args.empty()) {
                    std::string buf;
                    spdlog::details::registry::instance().apply_all(
                            [&](const std::shared_ptr<spdlog::logger>& logger) {
                                auto name = logger->name();
                                if (name.empty()) { name = "_default_"; }

                                ref->write(buf < ("{} = {}\n"_fmt
                                                  % name
                                                  % spdlog::level::to_string_view(logger->level())));
                            });
                } else if (args.size() == 1) {
                    spdlog::set_level(spdlog::level::from_str(std::string{args[0]}));
                } else {
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
        for (auto const& tracer : tracer::all()) {
            repos.insert(tracer->name());
        }
    }

    bool invoke(args_view args)
    {
        if (args.empty() || args.size() > 3) { return help(), false; }

        if (_async.valid()) {
            auto r_wait = _async.wait_for(200ms);

            if (r_wait == std::future_status::timeout) {
                SPDLOG_LOGGER_WARN(glog(), "previous trace lookup request is under progress ...");
                return false;
            } else if (r_wait == std::future_status::ready) {
                try {
                    _async.get();
                } catch (std::regex_error& e) {
                    SPDLOG_LOGGER_WARN(glog(), "regex error on trace filter: {}", e.what());
                }
            } else {
                throw std::logic_error("async operation must not be deffered.");
            }
        }

        std::string pattern{".*"};
        std::optional<bool> setter;

        if (args.size() > 1) { pattern.assign(args[1].begin(), args[1].end()); }
        if (args.size() > 2) {
            args[2] == "true" && (setter = true) || args[2] == "false" && (setter = false);
        }

        using namespace ranges;
        auto traces = tracer::all();

        auto it = std::find_if(traces.begin(),
                               traces.end(),
                               [&](auto& p) {
                                   return p->name() == args[0];
                               });

        if (it == traces.end()) {
            SPDLOG_LOGGER_ERROR(glog(), "name '{}' is not valid tracer name", args[0]);
            return false;
        }

        auto trc = *it;
        _async = std::async(std::launch::async, [=] { _async_request(trc, pattern, setter); });
        return true;
    }

   private:
    void _async_request(std::shared_ptr<tracer> ref, std::string pattern, std::optional<bool> setter)
    {
        std::promise<perfkit::tracer::fetched_traces> promise;
        auto fut = promise.get_future();
        auto valid_marker = std::make_shared<nullptr_t>();

        ref->on_fetch
                += [promise = &promise,
                    valid = std::weak_ptr{valid_marker}]  //
                (tracer::trace_fetch_proxy const& proxy) {
                    tracer::fetched_traces traces;
                    traces.reserve(64);
                    proxy.fetch_tree(&traces);

                    if (not valid.expired())
                        promise->set_value(traces);

                    return false;
                };

        ref->request_fetch_data();

        if (fut.wait_for(3s) == std::future_status::timeout) {
            SPDLOG_LOGGER_ERROR(glog(), "tracer '{}' update timeout", ref->name());
            return;
        }

        tracer::fetched_traces result = fut.get();

        using namespace ranges;
        std::regex match{pattern};
        std::string output;
        output << "\n"_fmt.s();

        array_view<std::string_view> current_hierarchy = {};
        std::string hierarchy_key, data_str, full_key;
        for (auto& item : result) {
            auto hierarchy = item.hierarchy.subspan(0, item.hierarchy.size() - 1);

            full_key.clear();
            for (auto c : item.hierarchy | views::join("."sv)) { full_key += c; }

            if (not std::regex_match(full_key, match)) { continue; }
            if (setter) { item.subscribe(*setter); }

            bool hierarchy_changed = hierarchy != current_hierarchy;
            if (hierarchy_changed) {
                current_hierarchy = hierarchy;
                hierarchy_key.clear();
                for (auto c : hierarchy | views::join("."sv))
                    hierarchy_key += c;

                if (not hierarchy_key.empty()) { hierarchy_key += '.'; }
            }

            // append string
            if (hierarchy_changed) {
                output << "{}{}"_fmt % hierarchy_key % item.key;
            } else {
                output << "{0:{1}}{2}"_fmt % "" % hierarchy_key.size() % item.key;
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
    register_config_manip_command(ref);
}

void register_config_manip_command(if_terminal* ref, std::string_view cmd)
{
    (void)0;  // deprecated!
}
}  // namespace perfkit::terminal

struct perfkit::if_terminal::impl {
    commands::registry commands;

    std::thread worker_stdin;
    std::atomic_bool is_shutting_down = false;
    std::once_flag flag_first_fetch_command;
};

void perfkit::if_terminal::invoke_command(std::string s)
{
    commands()->invoke_command(std::move(s));
}

void perfkit::if_terminal::_add_subcommand(
        std::string name, perfkit::if_terminal::cmd_invoke_function handler)
{
    commands()->root()->add_subcommand(std::move(name), std::move(handler));
}

perfkit::if_terminal::if_terminal() : _self(make_unique<impl>())
{
}

perfkit::commands::registry* perfkit::if_terminal::commands()
{
    return &_self->commands;
}

namespace perfkit::platform {
bool poll_stdin(int milliseconds, std::string* out_content);
}

void perfkit::if_terminal::launch_stdin_fetch_thread()
{
    // Multiple call to this function is permitted, but ignored.
    if (_self->worker_stdin.joinable()) { return; }

    auto fnWork
            = [this] {
                  std::string buffer;

                  do {
                      auto do_push = platform::poll_stdin(500, &buffer);
                      if (_self->is_shutting_down) { break; }
                      if (do_push) { push_command(buffer), buffer.clear(); }
                  } while (true);
              };

    _self->worker_stdin = std::thread{fnWork};
}

perfkit::if_terminal::~if_terminal()
{
    _self->is_shutting_down = true;
    if (_self->worker_stdin.joinable()) { _self->worker_stdin.join(); }
}

void perfkit::if_terminal::_call_once_init_stdin_receiver()
{
    call_once(_self->flag_first_fetch_command, &if_terminal::launch_stdin_fetch_thread, this);
}
