//
// Created by Seungwoo on 2021-09-30.
//

#include "net_terminal.hpp"

#include <utility>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include "net_session.hpp"
#include "perfkit/_net/net-proto.hpp"
#include "perfkit/detail/base.hpp"

#if __unix__ || __linux__
#include <sys/poll.h>
#include <unistd.h>
#endif

using namespace std::literals;

namespace perfkit::net {
perfkit::commands::registry* net_terminal::commands() {
  return &_commands;
}

std::optional<std::string> net_terminal::fetch_command(std::chrono::milliseconds timeout) {
  if (std::unique_lock lock{_cmd_queue_lock};
      _cmd_queue_notify.wait_for(lock, timeout, [&] { return not _cmd_queue.empty(); })) {
    // Retrieve single command from net-command-queue.
    // Pending commands in queue are always preceded.
    return _cmd_queue.dequeue();
  }

  return {};
}

void net_terminal::push_command(std::string_view command) {
  std::lock_guard _{_cmd_queue_lock};
  _cmd_queue.emplace_rotate(command);
}

void net_terminal::write(std::string_view str, perfkit::termcolor fg, perfkit::termcolor bg) {
  std::lock_guard _{_session_lock};

  if (_session == nullptr || not _session->is_connected()) {
    // if session is disconnected, put texts into buffer.
    auto oit = std::back_inserter(_text_buffer);
    if (fg != _prev_fg) { fg.append_xterm_256(oit, true); }
    if (bg != _prev_bg) { bg.append_xterm_256(oit, false); }
    std::copy(str.begin(), str.end(), oit);
  } else {
    // else, put incoming text into session directly.
    char buf[16];
    if (fg != _prev_fg) { fg.append_xterm_256(buf, true), _session->write(buf); }
    if (bg != _prev_bg) { bg.append_xterm_256(buf, false), _session->write(buf); }
    _session->write(str);
  }

  _prev_fg = fg;
  _prev_bg = bg;
}

std::shared_ptr<spdlog::sinks::sink> net_terminal::sink() {
  return _log_sink;
}

namespace details {
class net_sink_mt : public spdlog::sinks::base_sink<std::mutex> {
  using super = spdlog::sinks::base_sink<std::mutex>;

 public:
  net_sink_mt(net_terminal* owner, std::weak_ptr<bool> alive) : _owner{owner}, _alive(alive) {}

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    auto lock = _alive.lock();

    // log_msg is a struct containing the log entry info like level, timestamp, thread id etc.
    // msg.raw contains pre formatted log

    // If needed (very likely but not mandatory), the sink formats the message before sending it to its final destination:
    spdlog::memory_buf_t formatted;
    super::formatter_->format(msg, formatted);

    if (not lock) {
      fmt::format("WARNING: Server is closed.\n");
      fmt::format("message: {}", to_string(formatted));
    } else {
      _owner->write({formatted.begin(), formatted.size()});
    }
  }

  void flush_() override {}

 private:
  if_terminal* const _owner;
  std::weak_ptr<bool> _alive;
};

}  // namespace details

net_terminal::net_terminal(terminal::net_provider::init_info info)
        : _init_cached{std::move(info)} {
  _logger = spdlog::get("PERFKIT-NET");
  if (not _logger) {
    _logger = spdlog::default_logger()->clone("PERFKIT-NET");
    spdlog::register_logger(_logger);
  }

  _async_loop = std::thread{[&] { _worker_session_management(); }};
  _log_sink   = std::make_shared<details::net_sink_mt>(this, _alive_flag);

  if (info.wait_connection) {
    // if `wait_connection` flag is specified, wait until the first valid connection.
    for (;; std::this_thread::sleep_for(50ms)) {
      std::lock_guard _{_session_lock};
      if (_session && _session->is_connected()) { break; }
    }
  }

  _fd_source_stdout = dup(STDOUT_FILENO);
  _fd_source_stderr = dup(STDERR_FILENO);

  pipe(_fd_stdout);
  pipe(_fd_stderr);

  dup2(_fd_stdout[1], STDOUT_FILENO);
  dup2(_fd_stderr[1], STDERR_FILENO);

  _async_stdout_loop = std::thread{[&] { _worker_fetch_stdout(); }};
}

net_terminal::~net_terminal() {
  while (not _alive_flag.unique()) { std::this_thread::yield(); }
  _alive_flag.reset();
  _active.store(false);
  if (_async_loop.joinable()) { _async_loop.join(); }
  if (_async_stdout_loop.joinable()) { _async_stdout_loop.join(); }

  dup2(_fd_source_stdout, STDOUT_FILENO);
  dup2(_fd_source_stderr, STDERR_FILENO);

  ::close(_fd_stdout[0]), ::close(_fd_stdout[1]);
  ::close(_fd_stderr[0]), ::close(_fd_stderr[1]);
}

void net_terminal::_worker_session_management() {
  size_t n_conn_err_retry = 0;
  net_session::arg_poll pollargs;
  pollargs.command_registry = commands();
  pollargs.enqueue_command =
          [&](auto&& sv) {  // this puts incoming message to command queue
            (std::unique_lock{_cmd_queue_lock}, _cmd_queue.enqueue(sv));
            _cmd_queue_notify.notify_one();
          };

  // primary loop which manages lifecycles of each session, and listens to incoming messages
  while (_active) {
    if (_session == nullptr || not _session->is_connected()) {
      // if session is not initialized or died, reinitialize.
      (std::lock_guard{_session_lock}, _session.reset());

      auto ptr = std::make_unique<net_session>(&_init_cached);
      {
        std::lock_guard _{_session_lock};
        _session = std::move(ptr);

        // flush buffered text to newly established connection.
        _prev_fg = _prev_bg = {};
        _text_buffer.flat([&](char* begin, char* end) {
          _session->write({begin, size_t(end - begin)});
        });
        _text_buffer.clear();
      }
    }

    if (not _session->is_connected()) {
      n_conn_err_retry += 1;
      SPDLOG_LOGGER_ERROR(_logger, "failed to connect perfkit server: retry {}", n_conn_err_retry);
      continue;
    } else {
      n_conn_err_retry = 0;
    }

    _session->poll(pollargs);
  }
}

void net_terminal::_worker_fetch_stdout() {
  pollfd pollee[2];
  pollee[0].events = POLLIN;
  pollee[0].fd     = _fd_stdout[0];
  pollee[1].events = POLLIN;
  pollee[1].fd     = _fd_stderr[0];

  int dests[] = {_fd_source_stdout, _fd_source_stderr};

  char buf[1024];
  while (_active) {
    for (auto& pfd : pollee) { pfd.revents = 0; }

    auto num_ready = ::poll(pollee, 1, 250);
    if (num_ready == 0)
      continue;

    if (num_ready < 0) {
      SPDLOG_LOGGER_ERROR(_logger, "Polling STDOUT FAILED: {}", strerror(errno));
    }

    auto source = &*dests;
    for (auto const& pfd : pollee) {
      if (not(pfd.revents & POLLIN))
        continue;

      size_t n_read = ::read(pfd.fd, buf, sizeof buf);
      if (n_read == 0 || n_read == ~size_t{})
        continue;

      ((if_terminal*)this)->write({buf, n_read});
      ::write(*source++, buf, n_read);
    }
  }
}

}  // namespace perfkit::net
