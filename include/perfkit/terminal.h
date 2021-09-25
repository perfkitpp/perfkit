#pragma once
#include <chrono>
#include <memory>
#include <optional>

#include <spdlog/fwd.h>

namespace perfkit {
namespace commands {
class registry;
}

class if_terminal;
using std::chrono::milliseconds;

/** An exception that is thrown when user requests program termination */
struct termination : std::exception {};

/** Basic commandline terminal */
auto create_basic_interactive_terminal() -> std::shared_ptr<if_terminal>;

/** Net provider terminal */
auto create_net_provider_terminal(
        std::string_view server_ip,
        uint16_t server_port)
        -> std::shared_ptr<if_terminal>;

/**
 * Provides common user interface functionality for control purpose
 */
class if_terminal {
 public:
  /**
   * Reference to registered command registry.
   *
   * @return reference to registered command registry.
   */
  virtual commands::registry* commands() = 0;

  /**
   * Consume single command from user command input queue.
   *
   * @param timeout seconds to wait until receive command.
   * @return valid optional string, if dequeueing was successful.
   */
  virtual std::optional<std::string> fetch_command(milliseconds timeout = {}) = 0;

  /**
   * Put string to terminal
   */
  virtual void puts(std::string_view str) = 0;

  /**
   *
   */
  virtual std::shared_ptr<spdlog::sinks::sink> sink() = 0;

  /**
   * Property manipulations
   */
  virtual bool set(std::string_view key) { return false; };
  virtual bool set(std::string_view key, std::string_view value) { return false; };
  virtual bool set(std::string_view key, double value) { return false; };

  virtual bool get(std::string_view key, std::string_view* out) { return false; };
  virtual bool get(std::string_view key, double* out) { return false; };
};

}  // namespace perfkit