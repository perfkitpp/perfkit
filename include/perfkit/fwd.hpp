#pragma once

namespace perfkit {
namespace detail {
class config_base;
}

class config_registry;
class tracer;
class tracer_proxy;

class if_terminal;

template <typename Ty_>
class config;

namespace commands{
class registry;
}
}  // namespace perfkit
