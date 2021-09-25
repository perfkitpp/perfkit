#pragma once
#include <spdlog/fwd.h>

#include <chrono>
#include <memory>

#include "detail/command_registry.hpp"

namespace perfkit {
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
   * Register custom command registry.
   *
   * @param registry reference to registry
   * @return reference to existing command registry.
   */
  virtual auto reset_command_registery(std::unique_ptr<util::command_registry> registry)
          -> std::unique_ptr<util::command_registry> = 0;

  /**
   * Reference to registered command registry.
   *
   * @return reference to registered command registry.
   */
  virtual util::command_registry* commands() = 0;

  /**
   * Consume single command from user command input queue.
   *
   * @param timeout seconds to wait until receive command.
   * @return valid optional string, if dequeueing was successful.
   */
  virtual std::optional<std::string> dequeue_user_command(milliseconds timeout = {}) = 0;

  /**
   * Put string to terminal
   */
  virtual void puts(std::string_view str) = 0;

  /**
   *
   */
  virtual std::shared_ptr<spdlog::sinks::sink> sink() = 0;
};

}  // namespace perfkit