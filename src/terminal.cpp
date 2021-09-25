//
// Created by Seungwoo on 2021-09-25.
//
#include <filesystem>

#include <perfkit/detail/base.hpp>
#include <perfkit/detail/commands.hpp>
#include <perfkit/terminal.h>

#include "detail/basic_interactive_terminal.hpp"

namespace perfkit {
auto create_basic_interactive_terminal() -> std::shared_ptr<if_terminal> {
  return std::make_shared<basic_interactive_terminal>();
}
}  // namespace perfkit

namespace perfkit::terminal {
using namespace commands;

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
          cmd_load,
          [manip](auto&& tok) { return manip->load_from(tok); },
          [manip](auto&& tok, auto&& set) { return manip->retrieve_filenames(tok, set); });

  auto node_save = rootnode->add_subcommand(
          cmd_store,
          [manip](auto&& tok) { return manip->save_to(tok); },
          [manip](auto&& tok, auto&& set) { return manip->retrieve_filenames(tok, set); });

  if (!node_load || !node_save) { throw command_already_exist_exception{}; }
}

void register_logging_manip_command(if_terminal* ref, std::string_view cmd) {
}

void register_trace_manip_command(if_terminal* ref, std::string_view cmd) {
}

void register_config_manip_command(if_terminal* ref, std::string_view cmd) {
}

void initialize_with_basic_commands(if_terminal* ref) {
  register_logging_manip_command(ref);
  register_trace_manip_command(ref);
  register_conffile_io_commands(ref);
  register_config_manip_command(ref);
}

}  // namespace perfkit::terminal
