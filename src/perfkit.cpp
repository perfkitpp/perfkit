//
// Created by Seungwoo on 2021-08-29.
//
#include "perfkit/perfkit.h"

#include <fstream>

#include "perfkit/ui.hpp"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

namespace perfkit::ui {
if_ui* g_ui = nullptr;
}

bool perfkit::import_options(std::filesystem::path srcpath) {
  try {
    std::ifstream src{srcpath};
    if (!src) { return spdlog::critical("not a valid configuration file"), false; }
    nlohmann::json json = nlohmann::json::parse(std::istream_iterator<char>{src},
                                                std::istream_iterator<char>{},
                                                nullptr);

    auto& opts = json["___OPTIONS___"];
    if (opts.empty()) { return spdlog::critical("invalid json file specified."), false; }

    glog()->info("loaded config file [{}] successfully.", srcpath.string());

    for (auto& [name, ptr] : option_registry::all()) {
      auto strname = std::string(name);
      auto it      = opts.find(strname);
      if (it == opts.end()) {
        glog()->debug("option [{}] is not listed.", name);
        continue;
      }

      auto r_req = option_registry::request_update_value(std::move(strname), *it);
      if (!r_req) {
        glog()->critical("FOUND KEY {} IS MISSING FROM GLOBAL LIST. SOMETHING GONE WRONG !!!", name);
        std::terminate();
      }

      glog()->debug("IMPORTING: {} << {}", name, *it);
    }

    if (ui::g_ui) { ui::g_ui->on_import(srcpath); }
    return true;

  } catch (nlohmann::json::parse_error& e) {
    spdlog::critical("failed to parse json. ERRC: {}, at: {}, what(): {}", e.id, e.byte, e.what());
  }

  return false;
}

bool perfkit::export_options(std::filesystem::path dstpath) {
  if (ui::g_ui) { ui::g_ui->on_export(dstpath); }

  std::ofstream dst{dstpath};
  if (!dst) {
    glog()->error("failed to open file {}", dstpath.string());
    return false;
  }

  nlohmann::json exported;
  auto&          obj = exported["___OPTIONS___"];

  for (auto const& [name, ptr] : option_registry::all()) {
    obj[std::string(name)] = ptr->serialize();
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
