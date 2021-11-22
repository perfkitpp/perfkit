#pragma once
#include <spdlog/fwd.h>

namespace perfkit {
namespace detail {
class config_base;
}

class config_registry;
class tracer;
class tracer_proxy;

struct config_class;

namespace gui {
class instance;
}  // namespace gui

class if_terminal;

template <typename Ty_>
class config;

namespace commands {
class registry;
}
}  // namespace perfkit
