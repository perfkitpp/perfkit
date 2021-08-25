#pragma once
#include <functional>

#include "detail/array_view.hxx"

namespace perfkit::tui {

struct init_options {
  bool support_mouse = true;
};

void init(init_options const& opts = {});

void dispose();

void poll_once(bool update_messages = true);

void exec(int interval_ms, int* kill_switch, int message_update_cycle = 1);

void register_command(
    char const* first_keyword,
    std::function<void(std::string_view)> exec_handler,
    std::function<
        void(std::string_view cmd,
             std::vector<std::string_view>& candidates)>
        autucomplete_handler = {});

void tokenize_by_rule(std::string_view, std::vector<std::string_view>&);

}  // namespace perfkit::tui