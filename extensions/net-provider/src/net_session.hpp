//
// Created by Seungwoo on 2021-10-02.
//

#pragma once
#include <chrono>
#include <memory>
#include <optional>

#include "perfkit/detail/color.hxx"
#include "perfkit/extension/net-provider.hpp"

namespace perfkit::commands {
class registry;
}

namespace perfkit::net {

struct _sock_release {
  void operator()(void* p) noexcept { throw; }
};

using socket_ty = std::unique_ptr<void*, _sock_release>;

class net_session {
 public:
  perfkit::commands::registry* commands();
  std::optional<std::string> fetch_command(std::chrono::milliseconds timeout);
  void push_command(std::string_view command);
  void write(std::string_view str, perfkit::color fg, perfkit::color bg);

  bool is_connected() const noexcept;

 public:
  socket_ty _sock;
};
}  // namespace perfkit::net