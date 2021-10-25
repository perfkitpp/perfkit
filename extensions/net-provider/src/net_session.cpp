//
// Created by Seungwoo on 2021-10-02.
//

#include "net_session.hpp"

#include <thread>

#include <spdlog/spdlog.h>

#include "perfkit/detail/base.hpp"
#include "perfkit/detail/commands.hpp"

#if __linux__ || __unix__
#include <arpa/inet.h>
#include <perfkit/detail/configs.hpp>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/unistd.h>

using pollfd_ty = pollfd;
using std::lock_guard;
using std::unique_lock;

#elif _WIN32
#define _CRT_NONSTDC_NO_WARNINGS
#include <Winsock2.h>
#include <process.h>

#endif

using namespace std::literals;

template <typename Ty_>
std::optional<Ty_> perfkit::net::net_session::_retrieve(std::string_view s) {
  try {
    return _net::json::from_bson(s).get<Ty_>();
  } catch (_net::json::parse_error& e) {
    SPDLOG_WARN("parse error: ({}:{}) {}", e.id, e.byte, e.what());
  } catch (std::exception& e) {
    SPDLOG_WARN("conversion error: {}", e.what());
  }

  return {};
}

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
  _logger = spdlog::get("PERFKIT-NET");

  SPDLOG_LOGGER_INFO(_logger,
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
    SPDLOG_LOGGER_ERROR(_logger, "connect() failed. ({}) {}", errno, strerror(errno));
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
    auto n_read  = ::recv(sock, &(*to)[offset], to_recv, 0);
    if (n_read < 0) {
      SPDLOG_LOGGER_CRITICAL(
              glog(), "failed to receive, something gone wrong! ({}) {}",
              errno, strerror(errno));
      throw connection_error{};
    }
    if (n_read == 0) {
      SPDLOG_LOGGER_INFO(glog(), "disconnected from server.");
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
  _poll_context   = &arg;

  try {
    _bufmem.clear();

    auto phead = try_recv_as<_net::message_header>(_sock, &_bufmem, ms_to_wait);
    if (_bufmem.empty() || 0 != ::memcmp(phead->header, _net::HEADER.data(), _net::HEADER.size())) {
      SPDLOG_LOGGER_CRITICAL(_logger, "invalid packet header received ... content: {}",
                             std::string_view{phead->header, sizeof phead->header});
      _connected = false;
      return;
    }

    auto head = *phead;
    SPDLOG_LOGGER_TRACE(
            _logger, "header info: type {:02x}, payload {} bytes",
            head.type._value, head.payload_size);
    auto payload = try_recv(_sock, &_bufmem, head.payload_size, ms_to_wait);

    switch (head.type.server) {
      case _net::server_message::heartbeat: _send_heartbeat(); break;
      case _net::server_message::shell_enter: _handle_shell_input(payload); break;
      case _net::server_message::session_flush_request: _handle_flush_request(); break;

      default:
      case _net::server_message::invalid:
        _connected = false;
        SPDLOG_LOGGER_CRITICAL(
                _logger, "invalid packet header received ... type: 0x{:08x}", head.type.server);
        return;
    }
  } catch (connection_error&) {
    _connected = false;
    SPDLOG_LOGGER_ERROR(
            _logger, "[{}:{}] connection error ... ({}) {}",
            (void*)this, _sock, errno, strerror(errno));
  }
}

void perfkit::net::net_session::write(std::string_view str) {
  lock_guard _{_char_seq_lock};
  std::copy(str.begin(), str.end(), std::back_inserter(_chars_pending));
  _char_sequence += str.size();
}

void perfkit::net::net_session::_send_heartbeat() {
  SPDLOG_LOGGER_DEBUG(_logger, "heartbeat received.");
  std::lock_guard _{_sock_send_lock};
  _send(_msg_gen(_net::provider_message::heartbeat, {}));
}

void perfkit::net::net_session::_send(std::string_view payload) {
  ::send(_sock, payload.data(), payload.size(), 0);
}

void perfkit::net::net_session::_handle_flush_request() {
  SPDLOG_LOGGER_DEBUG(_logger, "shell fetch request received.");

  auto* msg  = &_reused_flush_chunk;
  msg->fence = ++_fence;

  msg->shell_content.clear();
  msg->config_registry_new.clear();
  msg->config_updates.clear();

  _handle_flush_request_SHELL(msg);
  _handle_flush_request_CONFIGS(msg);
  _handle_flush_request_TRACE(msg);
  _handle_flush_request_IMAGES(msg);

  {
    lock_guard _{_sock_send_lock};
    _send(_msg_gen(_net::provider_message::session_flush_reply, *msg));
  }
}

void perfkit::net::net_session::_handle_shell_input(perfkit::array_view<char> payload) {
  auto message = _retrieve<_net::shell_input_line>({payload.data(), payload.size()});
  if (not message) {
    SPDLOG_LOGGER_WARN(_logger, "invalid shell input message received.");
    return;
  }

  // if incoming message is invocation request, simply invoke it and forget it.
  if (not message->is_suggest) {
    _poll_context->enqueue_command(message->content);
    return;
  }

  // if incoming message is suggestion request, generate replacement.
  std::vector<std::string> candidates;
  auto str = _poll_context->command_registry->suggest(
          message->content, &candidates);

  _net::shell_suggest_reply reply;
  reply.content    = str.empty() && candidates.empty() ? message->content : std::move(str);
  reply.request_id = message->request_id;
  reply.suggest_words.reserve(candidates.size());

  std::transform(candidates.begin(), candidates.end(),
                 std::back_inserter(reply.suggest_words),
                 [](auto& m) { return std::move(m); });

  lock_guard{_sock_send_lock},
          _send(_msg_gen(reply));
}

void perfkit::net::net_session::_handle_flush_request_SHELL(
        perfkit::_net::session_flush_chunk* msg) {
  lock_guard _{_char_seq_lock};
  msg->shell_fence = _char_sequence;
  msg->shell_content.reserve(_chars_pending.size());
  _chars_pending.dequeue_n(_chars_pending.size(), std::back_inserter(msg->shell_content));
}

void perfkit::net::net_session::_handle_flush_request_CONFIGS(
        perfkit::_net::session_flush_chunk* msg) {
  // adjust update frequency
  if (_state_config.freq_timer.elapsed() < 0.5s)
    return;
  else
    _state_config.freq_timer.reset();

  // update fence
  msg->config_fence = ++_state_config.fence;

  // from all registries ...
  auto registries = perfkit::config_registry::bk_enumerate_registries();

  // find config registry insertions.
  for (auto const& rg : registries) {
    auto* monitoring             = &_state_config.monitoring_registries;
    bool const is_newly_inserted = monitoring->find(rg->name()) == monitoring->end();
    if (not is_newly_inserted)
      continue;

    // register newly detected config registry
    // - registry name
    // - all entities' metadata
    monitoring->emplace(rg->name());
    auto const& all_configs = rg->bk_all();
    auto* message           = &msg->config_registry_new.emplace_back();

    message->name = rg->name();
    message->entities.reserve(all_configs.size());

    for (const auto& [_, cfg] : all_configs) {
      auto entity                     = &message->entities.emplace_back();
      entity->hash                    = (intptr_t)cfg.get();
      entity->order_key               = cfg->full_key();
      entity->display_key             = cfg->display_key();
      entity->metadata                = cfg->attribute();
      entity->metadata["description"] = cfg->description();
    }
  }

  // from all configuration entities ...
  for (auto const& rg : registries) {
    // iterate all configurations ...
    for (const auto& [_, cfg] : rg->bk_all()) {
      auto markers      = &_state_config.update_markers;
      auto fence_update = cfg->num_modified() + cfg->num_serialized();
      auto [it, is_new] = markers->try_emplace((intptr_t)cfg.get(), 0);

      if (not is_new || it->second == fence_update)
        continue;  // there was no update.

      auto* entity = &msg->config_updates.emplace_back();
      entity->hash = (intptr_t)cfg.get();
      entity->data = cfg->serialize();

      // clear dirty flag.
      it->second = cfg->num_modified() + cfg->num_serialized();
    }
  }
}
