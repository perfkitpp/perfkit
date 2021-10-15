//
// Created by Seungwoo on 2021-08-29.
//
#include "perfkit/perfkit.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <perfkit/detail/base.hpp>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/spdlog.h"

std::string perfkit::_internal::INDEXER_STR(int order) {
  return fmt::format("{:->5}", order);
}

bool perfkit::import_options(std::string_view srcpath_s) {
  try {
    std::filesystem::path srcpath{srcpath_s};
    std::ifstream src{srcpath};
    if (!src) { return spdlog::critical("not a valid configuration file"), false; }
    nlohmann::json json = nlohmann::json::parse(
            ((std::stringstream&)(std::stringstream{} << src.rdbuf())).str(),
            nullptr);

    auto& opts = json["___OPTIONS___"];
    if (opts.empty()) { return spdlog::critical("invalid json file specified."), false; }

    glog()->info("loaded config file [{}] successfully.", srcpath.string());
    glog()->debug("\"___OPTIONS___\": {}", opts.dump(4));

    size_t total = 0, loaded = 0, skipped = 0;

    for (auto& [name, ptr] : config_registry::all()) {
      ++total;

      if (ptr->attribute().contains("transient")) {
        glog()->debug("skipping transient config '{}'", ptr->display_key());
        ++skipped;
        continue;
      }

      auto strname = ptr->display_key();
      auto it      = opts.find(strname);
      if (it == opts.end()) {
        glog()->debug("config [{}] is not listed.", name);
        continue;
      }

      auto r_req = config_registry::request_update_value(ptr->full_key(), *it);
      if (!r_req) {
        glog()->critical("FOUND KEY {} IS MISSING FROM GLOBAL LIST. SOMETHING GONE WRONG !!!", name);
        std::terminate();
      }

      ++loaded;
      glog()->debug("IMPORTING: {} << {}", name, it->dump());
    }

    glog()->info(
            "from {} configs ... {} loaded. {} skipped(transient). {} not listed in file",
            total, loaded, skipped, total - loaded - skipped);
    return true;

  } catch (nlohmann::json::parse_error& e) {
    spdlog::critical("failed to parse json. ERRC: {}, at: {}, what(): {}", e.id, e.byte, e.what());
  }

  return false;
}

bool perfkit::export_options(std::string_view dstpath_s) {
  std::filesystem::path dstpath{dstpath_s};

  std::ofstream dst{dstpath};
  if (!dst) {
    glog()->error("failed to open file {}", dstpath.string());
    return false;
  }

  dst << dump_options() << std::endl;
  glog()->info("configuration export to file [{}] is finished.", dstpath.string());

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

std::string perfkit::dump_options() {
  nlohmann::json exported;
  auto& obj = exported["___OPTIONS___"];

  for (auto const& [name, ptr] : config_registry::all()) {
    if (ptr->attribute().contains("transient")) {
      continue;
    }

    obj[std::string(ptr->display_key())] = ptr->serialize();
    glog()->debug("EXPORTING: {} >>> {}", name, ptr->serialize().dump());
  }

  return exported.dump(4);
}
