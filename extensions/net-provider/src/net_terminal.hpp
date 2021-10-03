//
// Created by Seungwoo on 2021-09-30.
//

#pragma once
#include <mutex>
#include <thread>

#include "perfkit/common/circular_queue.hxx"
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
  void _async_worker();

 private:
  std::mutex _session_lock;
  std::unique_ptr<net_session> _session;
  terminal::net_provider::init_info const _init_cached;

  std::thread _async_loop;
  std::atomic_flag _active{true};

  circular_queue<char> _text_buffer{512 * 1024 - 1};
  termcolor _prev_bg, _prev_fg;

  circular_queue<std::string> _cmd_queue{64};
  std::mutex _cmd_queue_lock;
};
}  // namespace perfkit::net
