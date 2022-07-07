#pragma once
#include <string_view>

#include "cpph/utility/template_utils.hxx"

namespace std::filesystem {
class path;
}

namespace perfkit {
using namespace cpph;
class if_terminal;

auto active_terminal() noexcept -> shared_ptr<if_terminal>;
bool is_actively_monitored() noexcept;

// TODO: Implement these!
bool add_directory_access_permission(std::string_view directory, bool allow_write = false);
bool add_directory_access_permission(std::filesystem::path const& directory, bool allow_write = false);
bool can_access_directory(std::filesystem::path const&, bool write_access = false) noexcept;
bool can_access_directory(std::string_view, bool write_access = false) noexcept;

// TODO: Implement this!
enum class client_noti_level {
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};

void client_noti(std::string_view, client_noti_level lv = client_noti_level::info);
bool client_noti_download(std::string_view filename);

// TODO: Register global command
}  // namespace perfkit