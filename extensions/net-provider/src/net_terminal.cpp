//
// Created by Seungwoo on 2021-09-30.
//

#include "net_terminal.hpp"

#include <utility>

#include <spdlog/spdlog.h>

#include "net_session.hpp"
#include "perfkit/_net/net-proto.hpp"
#include "perfkit/detail/base.hpp"

using namespace std::literals;

namespace perfkit::net {
perfkit::commands::registry* net_terminal::commands() {
  return nullptr;
}

std::optional<std::string> net_terminal::fetch_command(std::chrono::milliseconds timeout) {
  // Retrieve single command from net-command-queue.

  return std::optional<std::string>();
}

void net_terminal::push_command(std::string_view command) {
}

void net_terminal::write(std::string_view str, perfkit::termcolor fg, perfkit::termcolor bg) {

}

std::shared_ptr<spdlog::sinks::sink> net_terminal::sink() {
  return std::shared_ptr<spdlog::sinks::sink>();
}

net_terminal::net_terminal(terminal::net_provider::init_info info)
        : _init_cached{std::move(info)} {
  _async_loop = std::thread{[&] { _async_worker(); }};

  if (info.wait_connection) {
    // if `wait_connection` flag is specified, wait until the first valid connection.
    for (;; std::this_thread::sleep_for(50ms)) {
      std::lock_guard _{_session_lock};
      if (_session && _session->is_connected()) { break; }
    }
  }
}

net_terminal::~net_terminal() {
  _active.clear();
  if (_async_loop.joinable()) { _async_loop.join(); }
}

void net_terminal::_async_worker() {
  size_t n_conn_err_retry = 0;

  while (_active.test_and_set(std::memory_order_relaxed)) {
    if (_session == nullptr || not _session->is_connected()) {
      // if session is not initialized or died, reinitialize.
      std::lock_guard _{_session_lock};
      _session.reset();
      _session = std::make_unique<net_session>(&_init_cached);
    }

    if (not _session->is_connected()) {
      n_conn_err_retry += 1;
      SPDLOG_LOGGER_ERROR(glog(), "failed to connect perfkit server: retry {}", n_conn_err_retry);
      continue;
    } else {
      n_conn_err_retry = 0;
    }

    _session->poll();
  }
}

}  // namespace perfkit::net
