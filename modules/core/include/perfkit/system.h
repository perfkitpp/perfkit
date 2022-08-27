#pragma once
#include <cpph/std/string>

#include "cpph/utility/array_view.hxx"
#include "cpph/utility/functional.hxx"
#include "cpph/utility/generic.hxx"

namespace std::filesystem {
class path;
}

namespace perfkit {
using namespace cpph;
class if_terminal;

// TODO: Implement these!
void add_stdin_capture(weak_ptr<void>, ufunction<void(string_view)>);
void add_stdout_monitor(weak_ptr<void>, ufunction<void(string_view, bool is_err)>);
void remove_stdin_capture(weak_ptr<void>);
void remove_stdout_monitor(weak_ptr<void>);

//! Access to process own temporary directory ...
string const& access_tmpdir();

bool add_directory_access_permission(std::string_view directory, bool allow_write = false);
bool add_directory_access_permission(std::filesystem::path const& directory, bool allow_write = false);
bool has_directory_access_permission(std::string_view, bool write_access = false) noexcept;
bool has_directory_access_permission(std::filesystem::path const&, bool write_access = false) noexcept;

bool add_file_access_permission(std::string_view directory, bool allow_write = false);
bool add_file_access_permission(std::filesystem::path const& directory, bool allow_write = false);
bool has_file_access_permission(std::string_view, bool write_access = false) noexcept;
bool has_file_access_permission(std::filesystem::path const&, bool write_access = false) noexcept;

//! Binds system command ...
struct command_autocomplete_result;
struct command_autocomplete_request;

bool bind_system_command(
        array_view<string> category,
        ufunction<bool(string_view)> invoke_handler,
        ufunction<bool(command_autocomplete_request const&, command_autocomplete_result*)> = {});

struct command_autocomplete_request {
    string_view content;
    size_t cursor_pos;
    size_t initial_candidate_index;
    void (*add_candidate)(string_view);
};

struct command_autocomplete_result {
    size_t erase_offset = 0;
    size_t erase_count = 0;
    string replace_content;
};

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