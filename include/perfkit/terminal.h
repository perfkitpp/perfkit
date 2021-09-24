#pragma once
#include <chrono>
#include <memory>

#include "detail/command_registry.hpp"

namespace perfkit {
using std::chrono::milliseconds;

/** An exception that is thrown when user requests program termination */
struct termination : std::exception {};

/**
 * Provides common user interface functionality for control purpose
 */
class if_terminal {
 public:
  /**
   * Register command registry.
   *
   * @param registry reference to registry
   * @return reference to existing command registry.
   */
  virtual auto register_command_registery(std::unique_ptr<util::command_registry> registry)
          -> std::unique_ptr<util::command_registry> = 0;

  /**
   * Reference to registered command registry.
   *
   * @return reference to registered command registry.
   */
  virtual util::command_registry* commands() = 0;

  /**
   * Invoke any queued user commands
   *
   * @param timeout
   * @param dont_throw if set true,
   * @throws \refitem termination
   * @return number of invoked command
   */
  virtual size_t invoke_user_commands(milliseconds timeout = {}, bool dont_throw = false) = 0;

  /**
   * Put string to terminal
   */
  virtual void putstr(std::string_view str) = 0;
};
}  // namespace perfkit