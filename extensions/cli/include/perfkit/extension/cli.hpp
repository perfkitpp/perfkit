#pragma once
#include <perfkit/terminal.h>

namespace perfkit::terminal {
std::shared_ptr<if_terminal> create_cli();
}