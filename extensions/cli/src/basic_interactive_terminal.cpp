//
// Created by Seungwoo on 2021-09-25.
//

#include "basic_interactive_terminal.hpp"

#include <charconv>
#include <set>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "perfkit/common/format.hxx"
#include "perfkit/detail/base.hpp"

using namespace perfkit;
using namespace perfkit::literals;
using namespace std::literals;
using namespace fmt::literals;

#if _WIN32
#include <conio.h>
#elif __unix__ or __linux__
#include <linenoise.h>
#endif

#if _WIN32
using std::chrono::steady_clock;

basic_interactive_terminal::basic_interactive_terminal()
        : _sink{std::make_shared<spdlog::sinks::stdout_color_sink_mt>()} {
}

std::optional<std::string> basic_interactive_terminal::fetch_command(milliseconds timeout) {
  auto until = steady_clock::now() + timeout;
  std::optional<std::string> entered;

  if (std::unique_lock _{_cmd_queue_lock}; not _cmd_queued.empty()) {
    auto str = std::move(_cmd_queued.front());
    _cmd_queued.pop_front();
    return std::move(str);
  }

  do {
    if (_kbhit()) {
      char buf[4096];
      fgets(buf, sizeof buf, stdin);
      entered = buf;
      entered->pop_back();
      break;
    } else {
      std::this_thread::sleep_for(10ms);
    }
  } while (steady_clock::now() < until);

  return entered;
}

void basic_interactive_terminal::write(std::string_view str, color fg, color bg) {
  fwrite(str.data(), str.size(), 1, stdout);
}

void basic_interactive_terminal::push_command(std::string_view command) {
  std::unique_lock _{_cmd_queue_lock};
  _cmd_queued.emplace_back(command);
}

bool basic_interactive_terminal::set(std::string_view key, std::string_view value) {
  return if_terminal::set(key, value);
}

bool basic_interactive_terminal::get(std::string_view key, double *out) {
  return if_terminal::get(key, out);
}

#elif __unix__ or __linux__
std::optional<std::string>
perfkit::basic_interactive_terminal::fetch_command(
        std::chrono::milliseconds timeout) {
  if (!_cmd.valid()) {
    _cmd = std::async([this]() -> std::string {
      _register_autocomplete();
      auto c_str = linenoise(_prompt.c_str());
      _unregister_autocomplete();

      std::string retval;
      if (c_str) { retval = c_str, free(c_str); }

      return retval;
    });
  }

  if (auto _ = std::unique_lock{_cmd_queue_lock}; !_cmd_queued.empty()) {
    auto cmd = std::move(_cmd_queued.front());
    _cmd_queued.pop_front();
    return cmd;
  }

  if (_cmd.wait_for(timeout) != std::future_status::ready) {
    return {};
  }

  auto cmd = _cmd.get();
  if (cmd.empty() && !_cmd_history.empty()) {
    write("!{}\n"_fmt % _cmd_history.back() / 0, {}, {});
    cmd = _cmd_history.back();
  } else if (cmd.empty()) {
    return {};
  } else {
    bool is_history = cmd.front() == '!';

    if (!is_history && (_cmd_history.empty() || cmd != _cmd_history.back())) {
      _cmd_history.rotate(cmd);
      ++_cmd_counter;
    }
  }

  return cmd;
}

basic_interactive_terminal::basic_interactive_terminal() {
  _sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  spdlog::set_pattern("%+\r");
  char const *env_term = getenv("TERM");
  env_term == nullptr && (env_term = "<empty>");

  if (linenoiseIsUnsupportedTerm()) {
    glog()->warn("linenoise unsupported for $TERM={}", env_term);
  } else {
    glog()->info("$TERM={}", env_term);
  }

  _registry.root()->add_subcommand(
          "history",
          [this](auto &&) -> bool {
            auto pivot = _cmd_counter - _cmd_history.size();

            for (const auto &item : _cmd_history) {
              basic_interactive_terminal::write("{:5}  {}\n"_fmt(pivot++, item) / 0, {}, {});
            }

            return true;
          });

  _registry.add_invoke_hook(
          [this](std::string &s) {
            if (s.empty()) { return false; }
            if (s[0] == '!') {
              if (_cmd_history.empty()) { return false; }

              std::string tok = s;
              std::vector<std::string_view> tokens;
              commands::tokenize_by_argv_rule(&tok, tokens);

              tok.assign(tokens[0].begin() + 1, tokens[0].end());
              int hidx       = 0;
              auto r_conv    = std::from_chars(tok.c_str(), tok.c_str() + tok.size(), hidx);
              bool is_number = r_conv.ec != std::errc::invalid_argument
                            && r_conv.ptr == tok.c_str() + tok.size();

              if (is_number) {
                hidx = hidx - _cmd_counter + _cmd_history.size() + 1;
                if (hidx < 0 || hidx >= _cmd_history.size()) { return false; }

                s = _cmd_history.begin()[hidx];
              } else if (!tok.empty()) {
                auto it = std::find_if(_cmd_history.rbegin(), _cmd_history.rend(),
                                       [&, tok](auto &&ss) {
                                         return ss.find(tok) == 0;  // if contains !...
                                       });

                if (it == _cmd_history.rend()) { return false; }
                s = *it;
              } else {
                return false;
              }

              if (s != _cmd_history.back()) {
                _cmd_history.rotate(s);
              }

              basic_interactive_terminal::write("!{}\n"_fmt(s) / 0, {}, {});
            }

            linenoiseHistoryAdd(s.c_str());
            return true;
          });
}

bool basic_interactive_terminal::set(std::string_view key, std::string_view value) {
  std::string v{key};
  std::transform(v.begin(), v.end(), v.begin(), &tolower);

  if (v == "prompt") {
    _prompt = value;
  } else {
    return false;
  }

  return true;
}

static auto locked_command_registry = std::atomic<commands::registry *>{};

void basic_interactive_terminal::_register_autocomplete() {
  if (linenoiseIsUnsupportedTerm()) { return; }

  for (commands::registry *rg = nullptr;
       !locked_command_registry.compare_exchange_strong(rg, commands());
       rg = nullptr) {
    std::this_thread::sleep_for(10ms);
  }

  auto completion = [](const char *buf, linenoiseCompletions *lc) -> int {
    int position = 0;

    auto rg = locked_command_registry.load()->root();

    std::string str = buf;
    auto srclen     = str.size();
    std::vector<std::string_view> tokens;
    std::vector<commands::stroffset> offsets;
    std::vector<std::string> suggests;

    commands::tokenize_by_argv_rule(&str, tokens, &offsets);

    int target_token = 0;
    bool has_unique_match;
    rg->suggest(tokens, suggests,
                srclen > 0 && buf[srclen - 1] == ' ',
                &target_token, &has_unique_match);

    if (tokens.empty() == false) {
      if (target_token < tokens.size()) {
        // means matching was performed on existing token
        auto const &tokofst = offsets[target_token];

        position = tokofst.position;
        if (position > 0 && buf[position - 1] == '"') { position -= 1; }
      } else {
        // matched all existing tokens, thus returned suggests are for next word.
        // return last position of string for next completion.
        position = offsets.back().position + offsets.back().length + has_unique_match;

        if (buf[position - 1] == '"') { position += 1; }
      }
    }

    std::string suggest;
    for (const auto &suggest_src : suggests) {
      suggest = suggest_src;

      // escape all '"'s
      for (auto it = suggest.begin(); it != suggest.end(); ++it) {
        if (*it == '"') { it = suggest.insert(it, '\\'), ++it; }
      }

      if (suggest.find(' ') != ~size_t{}) {  // check
        linenoiseAddCompletion(lc, "\""s.append(suggest).append("\"").c_str());
      } else {
        linenoiseAddCompletion(lc, suggest.c_str());
      }
    }

    // glog()->info("target: {}, position: {}", target_token, position);
    return position;
  };

  linenoiseSetMultiLine(1);
  linenoiseSetCompletionCallback(completion);
}

void basic_interactive_terminal::_unregister_autocomplete() {
  linenoiseSetCompletionCallback(nullptr);
  locked_command_registry.store(nullptr);
}

void basic_interactive_terminal::push_command(std::string_view command) {
  std::unique_lock _{_cmd_queue_lock};
  _cmd_queued.emplace_back(command);
}

void basic_interactive_terminal::write(std::string_view str, termcolor fg, termcolor bg) {
  bool has_cr_lf = false;
  for (auto ch : str) {
    if (ch == '\n') {
      has_cr_lf |= 1;
      fputs("\r\n", stdout);
    } else {
      has_cr_lf |= ch == '\r';
      fputc(ch, stdout);
    }
  }

  if (has_cr_lf) { fflush(stdout); }
}

bool basic_interactive_terminal::get(std::string_view key, double *out) {
  if (key == "output-width") {
    *out = linenoiseNumTermCols + 1e-4;
  } else {
    return if_terminal::get(key, out);
  }

  return true;
}
#endif
