//
// Created by ki608 on 2021-11-21.
//

#pragma once
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <nlohmann/json.hpp>
#include <perfkit/common/algorithm/base64.hxx>
#include <perfkit/common/dynamic_array.hxx>
#include <perfkit/common/hasher.hxx>
#include <perfkit/common/helper/nlohmann_json_macros.hxx>
#include <perfkit/detail/helpers.hpp>
#include <spdlog/spdlog.h>

namespace perfkit::terminal::net::detail {
using asio::ip::tcp;
using socket_id_t = perfkit::basic_key<class LABEL_socket_id_t>;

using recv_archive_type = nlohmann::json;
using send_archive_type = nlohmann::json;

struct basic_dispatcher_impl_init_info {
  size_t buffer_size = 4 << 20;  // units in bytes
};

#pragma pack(push, 4)
struct message_header_t {
  char identifier[4]    = {'o', '`', 'P', '%'};
  char base64_buflen[4] = {};
};
static_assert(sizeof(message_header_t) == 8);
#pragma pack(pop)

struct message_in_t {
  std::tuple<std::string, recv_archive_type> body;
  CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(message_in_t, body);
};

struct message_out_t {
  std::tuple<std::string, int64_t, recv_archive_type> body;
  CPPHEADERS_DEFINE_NLOHMANN_JSON_ARCHIVER(message_out_t, body);
};

class basic_dispatcher_impl {
 public:
  using init_info = basic_dispatcher_impl_init_info;

 public:
  virtual ~basic_dispatcher_impl() = default;

  basic_dispatcher_impl(init_info s)
          : _cfg(std::move(s)),
            _buffer(_cfg.buffer_size) {}

 public:
  void launch() {
    _alive     = true;
    _io_worker = std::thread{[&] {
      while (_alive) {
        refresh();

        _io.run(), CPPH_INFO("IO CONTEXT STOPPED");
      }

      cleanup();
    }};
  }

  void shutdown() {
    if (not _alive)
      return;

    _alive = true;
    if (_io_worker.joinable())
      _io_worker.join();
  }

  void register_recv(
          std::string route,
          std::function<bool(recv_archive_type const& parameter)> handler) {
    // TODO register router
  }

  void send(
          std::string_view route,
          int64_t fence,
          void* userobj,
          void (*payload)(send_archive_type*, void*)) {
    // iterate all active sockets and push payload
    send_archive_type ss;
    payload(&ss[2], userobj);  // inverse order to prevent triple reallocation
    ss[1] = fence;
    ss[0] = route;

    // TODO: send message
  }

 protected:
  // 워커 스레드에서, io_context가 초기화될 때마다 호출.
  // 구현에 따라, 서버 소켓을 바인딩하거나, 서버에 연결하는 등의 용도로 사용.
  virtual void refresh() {}

  // shutdown() 시 호출. 서버 accept 소켓 등을 정리하는 데 사용.
  virtual void cleanup() {}

 protected:  // deriving classes may call this
  auto* io() { return &_io; }

  void notify_new_connection(socket_id_t id, std::unique_ptr<tcp::socket> socket) {
    std::lock_guard lck{_mtx_modify};

    auto [it, is_new] = _sockets.try_emplace(id, std::move(socket));
    assert_(is_new);  // id must be unique!

    auto* sock = &*it->second;
    asio::async_read(
            *sock, _buf(8),
            [this, id, sock](auto&& a, auto&& b) { _handle_prelogin_header(id, sock, a, b); });
  }

  void close(socket_id_t id) {
    std::unique_lock lc{_mtx_modify};
    auto it = _sockets.find(id);
    assert_(it != _sockets.end());

    it->second->close();
    _sockets_active.erase(
            std::find(_sockets_active.begin(), _sockets_active.end(), &*it->second));
    _sockets.erase(it);
  }

 private:  // handlers
  size_t _handle_header(
          socket_id_t id, tcp::socket* sock, asio::error_code const& ec, size_t n_read) {
    if (ec || n_read != 8) {
      CPPH_WARN("failed to receive header from socket {}", id.value);
      return ~size_t{};
    }

    auto size = _retrieve_buffer_size(_buffer.as<message_header_t>());
    if (size == ~size_t{}) {
      CPPH_ERROR("socket {}: protocol error, closing connection ...");
      close(id);
      return ~size_t{};
    }

    return size;
  }

  void _handle_prelogin_header(
          socket_id_t id, tcp::socket* sock, asio::error_code const& ec, size_t n_read) {
    auto size = _handle_header(id, sock, ec, n_read);
    if (~size_t{} == size)
      return;

    asio::async_read(
            *sock, _buf(size),
            [this, id, sock](auto&& a, auto&& b) { _handle_prelogin_body(id, sock, a, b); });
  }

  void _handle_prelogin_body(
          socket_id_t id, tcp::socket* sock, asio::error_code const& ec, size_t n_read) {
    if (ec || n_read != 8) {
      CPPH_WARN("failed to receive header from socket {}", id.value);
      return;
    }

    // TODO: 항상 ok
    CPPH_WARN("LOGIN LOGIC UNIMPLEMENTED YET, ALWAYS ACCEPT AUTHENTICATION ...");

    auto lc{std::lock_guard{_mtx_modify}};
    _sockets_active.push_back(sock);

    asio::async_read(
            *sock, _buf(8),
            [this, id, sock](auto&& a, auto&& b) { _handle_active_header(id, sock, a, b); });
  }

  void _handle_active_header(
          socket_id_t id, tcp::socket* sock, asio::error_code const& ec, size_t n_read) {
    auto size = _handle_header(id, sock, ec, n_read);
    if (~size_t{} == size)
      return;

    asio::async_read(
            *sock, _buf(size),
            [this, id, sock](auto&& a, auto&& b) { _handle_active_body(id, sock, a, b); });
  }

  void _handle_active_body(
          socket_id_t id, tcp::socket* sock, asio::error_code const& ec, size_t n_read) {
    if (ec || n_read != 8) {
      CPPH_WARN("failed to receive header from socket {}", id.value);
      return;
    }

    // TODO: find message's route and call handler.

    asio::async_read(
            *sock, _buf(8),
            [this, id, sock](auto&& a, auto&& b) { _handle_active_header(id, sock, a, b); });
  }

 private:  // helpers
  asio::mutable_buffer _buf(size_t num) {
    assert_(num < _buffer.size());
    return {_buffer.data(), num};
  }

  size_t _retrieve_buffer_size(message_header_t h) {
    static constexpr message_header_t reference;

    // check header validity
    if (not std::equal(h.identifier, *(&h.identifier + 1), reference.identifier))
      return CPPH_WARN("identifier: {}", std::string_view{h.base64_buflen, 4}), ~size_t{};

    // check if header size does not exceed buffer length
    size_t decoded = 0;
    if (not base64::decode(h.base64_buflen, (char*)&decoded))
      return CPPH_WARN("decoding base64: {}", std::string_view{h.base64_buflen, 4}), ~size_t{};

    if (decoded >= _cfg.buffer_size)
      return CPPH_WARN("buffer size exceeds max: {} [max {}]",
                       decoded, _cfg.buffer_size),
             ~size_t{};

    return decoded;
  }

  spdlog::logger* CPPH_LOGGER() const { return _logger.get(); }

 private:
  init_info _cfg;
  asio::io_context _io{1};
  std::unordered_map<socket_id_t, std::unique_ptr<tcp::socket>> _sockets;
  std::vector<tcp::socket*> _sockets_active;

  perfkit::dynamic_array<char> _buffer;

  std::atomic_bool _alive{false};
  std::thread _io_worker;

  std::string _token;      // login token to compare with.
  std::mutex _mtx_modify;  // locks when modification (socket add/remove) occurs.

  std::map<std::string, std::function<bool(recv_archive_type const& parameter)>>
          _recv_routes;

  logger_ptr _logger = logging::find_or("PERFKIT:NET");
};
}  // namespace perfkit::terminal::net::detail