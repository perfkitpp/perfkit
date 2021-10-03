//
// Created by Seungwoo on 2021-10-02.
//

#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>

#include "perfkit/_net/net-proto.hpp"
#include "perfkit/common/circular_queue.hxx"
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
  struct arg_poll {
    commands::registry const* command_registry;
    std::function<void(std::string_view)> enqueue_command;
  };

 public:
  explicit net_session(terminal::net_provider::init_info const*);
  ~net_session();

 public:
  void write(std::string_view str);
  void poll(arg_poll const& arg);
  bool is_connected() const noexcept { return _connected.load(std::memory_order_relaxed); }

 private:
  void _send_all_config() {}

  void _send_heartbeat();
  void _handle_config_fetch(array_view<char> payload) {}
  void _handle_trace_fetch(array_view<char> payload) {}
  void _handle_shell_fetch(array_view<char> payload) {}
  void _handle_shell_input(array_view<char> payload) {}

  void _send(std::string_view payload);

 public:
  socket_ty _sock = {};
  std::mutex _sock_send_lock;

  terminal::net_provider::init_info const* _pinit{};
  _net::message_builder _msg_gen{};

  std::atomic_bool _connected;
  std::string _bufmem;

  size_t _char_sequence;
  circular_queue<char> _chars_pending{512 * 1024 - 1};
};
}  // namespace perfkit::net