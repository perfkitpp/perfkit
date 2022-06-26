#pragma once
#include "cpph/utility/template_utils.hxx"

namespace perfkit {
using namespace cpph;
class if_terminal;

auto active_terminal() noexcept -> shared_ptr<if_terminal>;
bool is_actively_monitored() noexcept;

// TODO: Register global command
}  // namespace perfkit