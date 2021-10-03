//
// Created by Seungwoo on 2021-10-02.
//

#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>

#include "perfkit/_net/net-proto.hpp"
#include "perfkit/detail/termcolor.hxx"
#include "perfkit/extension/net-provider.hpp"

namespace perfkit::commands {
class registry;
}

namespace perfkit::net {
#if __linux__ || __unix__
using socket_ty = int;
#elif _WIN32
using socket_ty = void*;
#endif

class net_session {
 public:
  using clock_ty = std::chrono::steady_clock;

 public:
  explicit net_session(terminal::net_provider::init_info const*);
  ~net_session();

 public:
  perfkit::commands::registry* commands();
  std::optional<std::string> fetch_command(std::chrono::milliseconds timeout);
  void push_command(std::string_view command);
  void write(std::string_view str);

  void poll();

  bool is_connected() const noexcept { return _connected.load(std::memory_order_relaxed); }

 private:
  void _dump_all_config() {}

 public:
  socket_ty _sock = {};
  terminal::net_provider::init_info const* _pinit{};
  _net::message_builder _msg_gen{};

  std::atomic_bool _connected;
  std::string _bufmem;

  clock_ty::time_point _last_recv;
};
}  // namespace perfkit::net