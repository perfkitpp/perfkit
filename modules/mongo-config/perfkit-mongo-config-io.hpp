#pragma once
#include <iostream>
#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace perfkit::mongo_ext {
using config_registry_storage_t = std::map<std::string, nlohmann::json, std::less<>>;
using global_config_storage_t = std::map<std::string, config_registry_storage_t, std::less<>>;

auto try_load_configs(
        char const* uri_str,
        char const* storage,
        char const* site,
        char const* name,
        char const* version,
        std::ostream* olog = &std::cerr)
        -> std::optional<global_config_storage_t>;

bool try_save_configs(
        char const* uri_str,
        char const* storage,
        char const* site,
        char const* name,
        char const* version,
        global_config_storage_t const& cfg,
        std::ostream* olog = &std::cerr);

}  // namespace perfkit::mongo_ext
