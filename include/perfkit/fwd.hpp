#pragma once

namespace perfkit {
namespace detail {
class config_base;
}

class config_registry;
class tracer;
class tracer_proxy;

namespace gui {
class instance;
}  // namespace window

class if_terminal;

template <typename Ty_>
class config;

namespace commands {
class registry;
}
}  // namespace perfkit
