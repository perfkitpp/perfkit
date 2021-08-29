//
// Created by Seungwoo on 2021-08-30.
//
#include "trace_command_handler.hpp"

#include <chrono>

#include "defines.h"
#include "perfkit/detail/configs.hpp"
#include "spdlog/fmt/bundled/ranges.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

using namespace perfkit::ui::detail;
using namespace perfkit::ui;
using namespace perfkit;
using namespace std::literals;

extern int COLS;

namespace {
auto& OPTS = config_registry::create();

auto opt_trace_fetch_interval
    = configure(OPTS, NS_DASHBOARD "Traces|+0|Update Interval", 0.033).confirm();
auto opt_list_output_width
    = configure(OPTS, NS_DASHBOARD "Traces|+0|Window Width", 60).confirm();
}  // namespace

template <typename Range_>
static void split_string_by(char const* delim, std::string_view str, Range_& o) {
  while (!str.empty()) {
    auto             where = str.find_first_of(delim);
    std::string_view tok;
    if (where == std::string_view::npos) {
      tok = str, str = {};
    } else {
      tok = str.substr(0, where);
      str = str.substr(where + 1);
    }

    o.push_back(tok);
  }
}

trace_command_handler::trace_command_handler(
    const perfkit::ui::detail::trace_command_handler::init_args& i)
    : _tracer(i.ref), _log(i.logging) {
  _flag_worker_fetch.test_and_set();
  _thrd_worker_fetch = std::thread{
      [this] {
        while (_flag_worker_fetch.test_and_set(std::memory_order_relaxed)) {
          OPTS.apply_update_and_check_if_dirty();
          auto wait  = 1ms * (int)(1000.ms * opt_trace_fetch_interval.get()).count();
          auto until = std::chrono::steady_clock::now() + wait;

          _check_update(wait);
          std::this_thread::sleep_until(until);
        }
      }};
}

bool trace_command_handler::on_list(args_view argv) {
  std::string_view key;
  if (!argv.empty()) { key = argv[0]; }
  if (!key.empty()) { key = key.substr(1); }  // discard first '$'

  std::vector<std::string_view> strlist;
  split_string_by("$", key, strlist);
  std::string buf, buf2;

  spdlog::trace("tokens {}", strlist);
  spdlog::trace("global COLS on this scope {}", COLS);

  // match all instances
  fmt::print("{} \n", key);

  for (const auto& item : _fetched) {
    array_view<std::string_view> view{strlist};

    bool match = view.size() <= item.hierarchy.size();
    for (size_t index = 0; index < view.size() && match; ++index) {
      match = (view[index] == item.hierarchy[index]);
    }

    if (match) {
      buf.clear(), item.dump_data(buf2);

      auto to_show = item.hierarchy.subspan(view.size());
      fmt::format_to(back_inserter(buf), "{:>{}}", "* ", item.hierarchy.size() * 2);
      for (const auto& show : to_show) { fmt::format_to(back_inserter(buf), "/{}", show); }

      fmt::format_to(back_inserter(buf), "{0:{1}}[{2}]\n",
                     "",
                     std::min<int>(opt_list_output_width.get(),
                                   (volatile int&)COLS - 2 - 7)
                         - buf.size() - buf2.size(),
                     buf2);
      fmt::print(buf);
    }
  }

  return true;
}

void trace_command_handler::on_suggest(
    args_view   hint,
    string_set& candidates) {
  auto _ = _lock();

  std::string hierarchy_str;
  for (const auto& item : _fetched) {
    hierarchy_str.clear();

    for (const auto& key : item.hierarchy) { hierarchy_str.append("$").append(key); }
    candidates.insert(hierarchy_str);

    _log->trace("Candidate Added #{} -> {}", item.hash, hierarchy_str);
  }
  _log->trace("Suggest Done.");
}

bool trace_command_handler::on_watch(args_view argv) {
  return false;
}

bool trace_command_handler::_check_update(std::chrono::milliseconds timeout) {
  if (!_pending_fetch.valid()) {
    _pending_fetch = _tracer->async_fetch_request();
    return false;
  }

  switch (_pending_fetch.wait_for(timeout)) {
    case std::future_status::ready: {
      {  // to copy safely ...
        auto _ = _lock();
        _pending_fetch.get().copy_sorted(_fetched);
      }
      _pending_fetch = _tracer->async_fetch_request();
      return true;
    }
    case std::future_status::timeout:
    case std::future_status::deferred:
    default:
      return false;
  }
}

trace_command_handler::~trace_command_handler() {
  _flag_worker_fetch.clear();
  _thrd_worker_fetch.join();
}
