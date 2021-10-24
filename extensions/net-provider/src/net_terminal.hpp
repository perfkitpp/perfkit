//
// Created by Seungwoo on 2021-09-30.
//

#pragma once
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <thread>

#include "perfkit/common/circular_queue.hxx"
#include "perfkit/detail/commands.hpp"
#include "perfkit/extension/net-provider.hpp"
#include "perfkit/terminal.h"

namespace perfkit::net {
class net_session;

class net_terminal : public perfkit::if_terminal {
 public:
  explicit net_terminal(terminal::net_provider::init_info);
  ~net_terminal();

 public:
  perfkit::commands::registry* commands() override;
  std::optional<std::string> fetch_command(std::chrono::milliseconds timeout) override;
  void push_command(std::string_view command) override;
  void write(std::string_view str, perfkit::termcolor fg, perfkit::termcolor bg) override;
  std::shared_ptr<spdlog::sinks::sink> sink() override;

 private:
  void _worker_session_management();
  void _worker_fetch_stdout();

 private:
  std::mutex _session_lock;
  std::unique_ptr<net_session> _session;
  terminal::net_provider::init_info const _init_cached;

  std::thread _async_loop;
  std::thread _async_stdout_loop;
  std::atomic_bool _active{true};

  circular_queue<char> _text_buffer{512 * 1024 - 1};
  termcolor _prev_bg, _prev_fg;

  circular_queue<std::string> _cmd_queue{64};
  std::mutex _cmd_queue_lock;
  std::condition_variable _cmd_queue_notify;

  perfkit::commands::registry _commands;

  std::shared_ptr<spdlog::sinks::sink> _log_sink;
  std::shared_ptr<bool> _alive_flag = std::make_shared<bool>();

  std::string _reused_stdout_buf;

  int _fd_stdout[2]     = {-1};
  int _fd_stderr[2]     = {-1};

  int _fd_source_stdout = -1;
  int _fd_source_stderr = -1;

};
}  // namespace perfkit::net
