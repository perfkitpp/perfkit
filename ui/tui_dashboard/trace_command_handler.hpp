//
// Created by Seungwoo on 2021-08-30.
//
#pragma once
#include "perfkit/detail/command_registry.hpp"
#include "perfkit/detail/tracer.hpp"
#include "spdlog/fwd.h"

namespace perfkit::ui::detail {
class trace_command_handler {
 public:
  struct init_args {
    tracer*         ref;
    spdlog::logger* logging;
  };

 public:
  explicit trace_command_handler(init_args const& i);

  bool on_list(ui::args_view argv);
  bool on_watch(ui::args_view argv);
  void on_suggest(args_view hint, string_set& argv);

  ~trace_command_handler();

 private:
  bool _on_cmd_impl(ui::args_view argv, int cmdtype);

  bool _check_update(std::chrono::milliseconds timeout = {});

 public:
  auto _lock() { return std::unique_lock{_fetch_lock}; }

 private:
  std::shared_future<tracer::async_trace_result> _pending_fetch = {};

  std::mutex             _fetch_lock;
  tracer::fetched_traces _fetched = {};
  tracer* const          _tracer;
  spdlog::logger* const  _log;

  std::atomic_flag _flag_worker_fetch;
  std::thread      _thrd_worker_fetch;
};
}  // namespace perfkit::ui::detail
