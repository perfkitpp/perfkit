//
// Created by Seungwoo on 2021-09-25.
//
#include <filesystem>

#include <perfkit/detail/base.hpp>
#include <perfkit/detail/commands.hpp>
#include <perfkit/detail/configs.hpp>
#include <perfkit/detail/format.hxx>
#include <perfkit/terminal.h>
#include <range/v3/algorithm.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/subrange.hpp>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include "detail/basic_interactive_terminal.hpp"
using namespace std::literals;

namespace perfkit {
auto create_basic_interactive_terminal() -> std::shared_ptr<if_terminal> {
  return std::make_shared<basic_interactive_terminal>();
}
}  // namespace perfkit

namespace perfkit::terminal {
using commands::args_view;
using commands::command_already_exist_exception;
using commands::command_name_invalid_exception;
using commands::string_set;

class _config_saveload_manager {
 public:
  bool load_from(args_view args = {}) {
    auto path = args.empty() ? _latest : args.front();
    _latest   = path;
    return perfkit::import_options(path);
  }

  bool save_to(args_view args = {}) {
    auto path = args.empty() ? _latest : args.front();
    _latest   = path;
    return perfkit::export_options(path);
  }

  void retrieve_filenames(args_view args, string_set& cands) {
    namespace fs = std::filesystem;

    fs::path path;
    if (!args.empty()) { path = args[0]; }
    if (path.empty()) { path = "./"; }

    if (!is_directory(path)) { path = path.parent_path(); }
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
        std::string_view initial_path) {
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

void register_logging_manip_command(if_terminal* ref, std::string_view cmd) {
  std::string cmdstr{cmd};

  struct fn_op {
    if_terminal* ref;
    std::string logger_name;

    bool operator()(args_view args) {
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

  auto logging = ref->commands()->root()->add_subcommand(
          cmdstr,
          {},
          [ref, cmdstr, fn_sugg](auto&&, auto&& cands) -> bool {
            auto node = ref->commands()->root()->find_subcommand(cmdstr);
            spdlog::details::registry::instance().apply_all(
                    [&](const std::shared_ptr<spdlog::logger>& logger) {
                      if (logger->name().empty()) { return; }
                      if (node->find_subcommand(logger->name())) { return; }
                      auto cmd = node->add_subcommand(logger->name(), fn_op{ref, logger->name()}, fn_sugg);
                    });

            return true;
          },
          true);

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

void register_trace_manip_command(if_terminal* ref, std::string_view cmd) {
}

class _config_manip {
 public:
  _config_manip(if_terminal* ref, config_ptr pt)
          : _ref(ref), _conf(pt) {}

  bool get(args_view) {
    _ref->write(("{} = {}\n"_fmt % _conf->display_key() % _conf->serialize().dump()) / 16);
    return true;
  }

  bool set(args_view tok) {
    if (tok.empty()) {
      SPDLOG_LOGGER_ERROR(glog(), "");
      return false;
    }

    return false;
  }

  bool info(args_view) {
    std::string buf{};
    buf.reserve(1024);

    buf.append("\n");
    buf << "name       > {}\n"_fmt % _conf->display_key();
    buf << "key        > {}\n"_fmt % _conf->full_key();
    buf << "description> {}\n"_fmt % _conf->description();
    buf << "attributes > {}\n"_fmt % _conf->attribute().dump(2);
    buf << "value      > {}\n"_fmt % _conf->serialize().dump(2);

    _ref->write(buf);
    return true;
  }

  bool toggle(args_view) {
    return false;
  }

 private:
  if_terminal* _ref;
  config_ptr _conf;
};

class _config_category_manip {
 public:
  explicit _config_category_manip(if_terminal* ref, std::string display, int depth)
          : _ref{ref}
          , _category{std::move(display).substr(1).append("|")}
          , _depth{depth} {
    ranges::replace(_category, '.', '|');
  }

  bool operator()(args_view) {
    // list all configurations belong to this category.
    using namespace ranges;
    auto all = &config_registry::all();

    std::string buf;
    buf.reserve(1024);

    // header
    {
      double width = 40;
      _ref->get("output-width", &width);

      buf << "{:-<{}}\n"_fmt % (_category + " ") % (int)width;
    }

    // during string begins with this category name ...
    for (const auto& [_, conf] : *all) {
      if (conf->display_key().find(_category) != 0) { continue; }

      auto depth = conf->tokenized_display_key().size();
      auto name  = conf->display_key().substr(_category.size());

      buf << "..|{} = {}\n"_fmt % name % conf->serialize().dump();
    }

    _ref->write(buf);
    return true;
  }

 private:
  if_terminal* _ref;
  std::string _category;
  int _depth;
};

void register_config_manip_command(if_terminal* ref, std::string_view cmd) {
  using namespace ranges;

  auto node_conf = ref->commands()->root()->add_subcommand(
          std::string{cmd},
          [cmdstr = std::string{cmd}](auto&&) -> bool {
            glog()->error("usage: {} <config_name> [set [<values...>]|toggle] ");
            return true;
          },
          {});

  for (const auto& [_, config] : config_registry::all()) {
    auto hierarchy = config->tokenized_display_key();

    // change display key expression into <a>.<b>.<c>
    auto key = subrange(hierarchy.begin(), hierarchy.end() - 1)
             | views::join('.')
             | to<std::string>();

    size_t category_len = key.size();
    (key.empty() ? key : key.append(".")).append(hierarchy.back());
    auto category_match = "*"s.append(std::string_view{key}.substr(0, category_len));

    // add category command
    if (!category_match.empty() && !node_conf->find_subcommand(category_match)) {
      auto category_name = hierarchy.end()[-2];

      auto manip = std::make_shared<_config_category_manip>(
              ref,
              std::string{category_match},
              hierarchy.size() - 1);

      auto node_cat = node_conf->add_subcommand(
              category_match,
              [=](auto&& tok) { return (*manip)(tok); });
    }

    // add manipulation commands
    auto manip = std::make_shared<_config_manip>(ref, config);
    auto node  = node_conf->add_subcommand(
             key,
             [=](auto&& s) { return manip->get(s); });

    node->add_subcommand("set", [=](auto&& s) { return manip->set(s); });
    node->add_subcommand("detail", [=](auto&& s) { return manip->info(s); });

    if (config->attribute()["default"].is_boolean()) {
      node->add_subcommand("toggle", [=](auto&& s) { return manip->toggle(s); });
    }
  }
}

void initialize_with_basic_commands(if_terminal* ref) {
  register_logging_manip_command(ref);
  register_trace_manip_command(ref);
  register_conffile_io_commands(ref);
  register_config_manip_command(ref);
}

}  // namespace perfkit::terminal
