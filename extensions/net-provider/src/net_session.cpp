//
// Created by Seungwoo on 2021-10-02.
//

#include "net_session.hpp"

#include <thread>

#include <spdlog/spdlog.h>

#include "perfkit/detail/base.hpp"

#if __linux__ || __unix__
#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/unistd.h>

using pollfd_ty = pollfd;
#elif _WIN32
#define _CRT_NONSTDC_NO_WARNINGS
#include <Winsock2.h>
#include <process.h>

#endif

using namespace std::literals;
perfkit::net::net_session::~net_session() {
  _connected = false;

  if (_sock) {
    SPDLOG_LOGGER_INFO(glog(), "[{}] closing socket {} ...", (void*)this, _sock);
    ::close(_sock);
  }
}

static int64_t g_epoch_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();

perfkit::net::net_session::net_session(
        perfkit::terminal::net_provider::init_info const* init)
        : _pinit{init} {
  SPDLOG_LOGGER_INFO(glog(),
                     "[{}] session '{}' opening connection to {}:{}",
                     (void*)this, _pinit->name,
                     _pinit->host_name, _pinit->host_port);
  // Open connection to server
  _sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (_sock == -1) { return; }

  sockaddr_in ep_host     = {};
  ep_host.sin_addr.s_addr = inet_addr(init->host_name.c_str());
  ep_host.sin_family      = AF_INET;
  ep_host.sin_port        = htons(init->host_port);

  if (0 != ::connect(_sock, (sockaddr*)&ep_host, sizeof ep_host)) {
    SPDLOG_LOGGER_ERROR(glog(), "connect() failed. ({}) {}", errno, strerror(errno));
    std::this_thread::sleep_for(1s);
    return;
  }

  // Send session info
  char buf[128];

  _net::session_register_info session;
  session.session_name = init->name;
  session.machine_name = (gethostname(buf, sizeof buf), buf);
  session.description  = init->description;
  session.pid          = getpid();
  session.epoch        = g_epoch_time;
  _send(_msg_gen(session));

  // Send all configs
  _send_all_config();

  // set as connected
  _connected = true;
}

namespace perfkit::net {
struct connection_error : std::exception {};
struct invalid_header_error : std::exception {};

array_view<char> try_recv(socket_ty sock, std::string* to, size_t size_to_recv, int timeout) {
  size_t received = 0;
  auto offset     = to->size();
  auto org_offset = offset;
  to->resize(to->size() + size_to_recv);

  while (received < size_to_recv) {
    pollfd_ty pollee{sock, POLLIN, 0};
    if (auto r_poll = ::poll(&pollee, 1, timeout); r_poll <= 0) {
      errno = ETIMEDOUT;
      SPDLOG_LOGGER_CRITICAL(glog(), "connection timeout; no data received for {} ms", timeout);
      throw connection_error{};
    }

    if (not(pollee.revents & POLLIN)) {
      SPDLOG_LOGGER_CRITICAL(glog(), "failed to poll, something gone wrong!");
      throw connection_error{};
    }

    auto to_recv = size_to_recv - received;
    auto n_read  = ::recv(sock, &to[offset], to_recv, 0);
    if (n_read < 0) {
      SPDLOG_LOGGER_CRITICAL(
              glog(), "failed to receive, something gone wrong! ({}) {}",
              errno, strerror(errno));
      throw connection_error{};
    }

    received += n_read;
    offset += n_read;  // move pointer ahead
  }

  return {to->data() + org_offset, received};
}

template <typename Ty_>
Ty_* try_recv_as(socket_ty sock, std::string* to, int timeout) {
  return reinterpret_cast<Ty_*>(try_recv(sock, to, sizeof(Ty_), timeout).data());
}

}  // namespace perfkit::net

void perfkit::net::net_session::poll(arg_poll const& arg) {
  auto ms_to_wait = _pinit->connection_timeout_ms.count();

  try {
    _bufmem.clear();

    auto head = try_recv_as<_net::message_header>(_sock, &_bufmem, ms_to_wait);
    if (0 != ::memcmp(head->header, _net::HEADER.data(), _net::HEADER.size())) {
      SPDLOG_LOGGER_CRITICAL(glog(), "invalid packet header received ... content: {}",
                             std::string_view{head->header, sizeof head->header});
      _connected = false;
      return;
    }

    auto payload = try_recv(_sock, &_bufmem, head->payload_size, ms_to_wait);

    switch (head->type.server) {
      case _net::server_message::heartbeat: _send_heartbeat(); break;
      case _net::server_message::config_fetch: _handle_config_fetch(payload); break;
      case _net::server_message::shell_input: _handle_shell_input(payload); break;
      case _net::server_message::shell_fetch: _handle_shell_fetch(payload); break;
      case _net::server_message::trace_fetch: _handle_trace_fetch(payload); break;

      default:
      case _net::server_message::invalid:
        _connected = false;
        SPDLOG_LOGGER_CRITICAL(
                glog(), "invalid packet header received ... type: 0x{:08x}", head->type.server);
        return;
    }
  } catch (connection_error&) {
    _connected = false;
    SPDLOG_LOGGER_ERROR(
            glog(), "[{}:{}] connection error ... ({}) {}",
            (void*)this, _sock, errno, strerror(errno));
  }
}

void perfkit::net::net_session::write(std::string_view str) {
}

void perfkit::net::net_session::_send_heartbeat() {
  _send(_msg_gen(_net::provider_message::heartbeat, {}));
}

void perfkit::net::net_session::_send(std::string_view payload) {
  std::lock_guard _{_sock_send_lock};
  ::send(_sock, payload.data(), payload.size(), 0);
}
