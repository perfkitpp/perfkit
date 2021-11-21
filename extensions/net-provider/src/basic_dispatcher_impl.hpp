//
// Created by ki608 on 2021-11-21.
//

#pragma once
#include <unordered_map>

#include <asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>
#include <perfkit/common/dynamic_array.hxx>
#include <perfkit/common/hasher.hxx>

namespace perfkit::terminal::net::detail {
using asio::ip::tcp;
using socket_id_t = perfkit::basic_key<class LABEL_socket_id_t>;

struct basic_dispatcher_impl_init_info {
  size_t buffer_size = 16 << 20; // 16 MB default
};

class basic_dispatcher_impl {
  using recv_archive_type = nlohmann::json;
  using send_archive_type = nlohmann::json;

 public:
  using init_info = basic_dispatcher_impl_init_info;

 public:
  virtual ~basic_dispatcher_impl() = default;
  basic_dispatcher_impl(init_info s)
          : _cfg(std::move(s)),
            _buffer(_cfg.buffer_size) {}

 public:
  void register_recv(
          std::string route,
          std::function<bool(recv_archive_type const& parameter)>) {}

  void send(
          std::string_view route,
          int64_t fence,
          send_archive_type payload) {}

 protected:
  auto* io() { return &_io; }
  void notify_new_connection(socket_id_t id, std::unique_ptr<tcp::socket> socket);

 private:
 private:
  init_info _cfg;
  asio::io_context _io{1};
  std::unordered_map<socket_id_t, std::unique_ptr<tcp::socket>> _sockets;
  perfkit::dynamic_array<char> _buffer;
};
}  // namespace perfkit::terminal::net::detail