//
// Created by Seungwoo on 2021-08-29.
//
#include "perfkit/perfkit.h"

#include <filesystem>
#include <fstream>

#include "perfkit/ui.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

namespace perfkit::ui {
if_ui* g_ui = nullptr;
}

std::string perfkit::_internal::INDEXER_STR(int order) {
  return fmt::format("{:->5}", order);
}

bool perfkit::import_options(std::string_view srcpath_s) {
  try {
    std::filesystem::path srcpath{srcpath_s};
    std::ifstream         src{srcpath};
    if (!src) { return spdlog::critical("not a valid configuration file"), false; }
    nlohmann::json json = nlohmann::json::parse(
        ((std::stringstream&)(std::stringstream{} << src.rdbuf())).str(),
        nullptr);

    auto& opts = json["___OPTIONS___"];
    if (opts.empty()) { return spdlog::critical("invalid json file specified."), false; }

    glog()->info("loaded config file [{}] successfully.", srcpath.string());
    glog()->debug("\"___OPTIONS___\": {}", opts.dump(4));

    for (auto& [name, ptr] : config_registry::all()) {
      auto strname = ptr->display_key();
      auto it      = opts.find(strname);
      if (it == opts.end()) {
        glog()->debug("config [{}] is not listed.", name);
        continue;
      }

      auto r_req = config_registry::request_update_value(std::string(ptr->full_key()), *it);
      if (!r_req) {
        glog()->critical("FOUND KEY {} IS MISSING FROM GLOBAL LIST. SOMETHING GONE WRONG !!!", name);
        std::terminate();
      }

      glog()->debug("IMPORTING: {} << {}", name, *it);
    }

    if (ui::g_ui) { ui::g_ui->on_import(srcpath_s); }
    return true;

  } catch (nlohmann::json::parse_error& e) {
    spdlog::critical("failed to parse json. ERRC: {}, at: {}, what(): {}", e.id, e.byte, e.what());
  }

  return false;
}

bool perfkit::export_options(std::string_view dstpath_s) {
  std::filesystem::path dstpath{dstpath_s};
  if (ui::g_ui) { ui::g_ui->on_export(dstpath_s); }

  std::ofstream dst{dstpath};
  if (!dst) {
    glog()->error("failed to open file {}", dstpath.string());
    return false;
  }

  nlohmann::json exported;
  auto&          obj = exported["___OPTIONS___"];

  for (auto const& [name, ptr] : config_registry::all()) {
    obj[std::string(ptr->display_key())] = ptr->serialize();
    glog()->debug("EXPORTING: {} >>> {}", name, ptr->serialize().dump());
  }

  dst << exported.dump(4) << std::endl;
  glog()->info("configuration export to file [{}] is done.", dstpath.string());

  return true;
}

std::shared_ptr<spdlog::logger> perfkit::glog() {
  static std::weak_ptr<spdlog::logger> _inst{};
  if (!_inst.lock()) {
    auto ptr = spdlog::get("PERFKIT");
    if (!ptr) { ptr = spdlog::stdout_color_mt("PERFKIT"); }
    _inst = ptr;
  }

  return _inst.lock();
}
