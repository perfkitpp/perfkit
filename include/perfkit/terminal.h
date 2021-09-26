#pragma once
#include <chrono>
#include <memory>
#include <optional>

#include <perfkit/detail/color.hxx>
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
   * Enqueue command to internal queue.
   *
   * This should appear in fetch_command();
   */
  virtual void push_command(std::string_view command) = 0;

  /**
   * Output string to terminal
   */
  virtual void write(std::string_view str, color color = {}) = 0;

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

namespace terminal {
/**
 * Register basic commands
 *
 * @param ref
 */
void initialize_with_basic_commands(if_terminal* ref);

/**
 * Register configuration load, store commands
 *
 * @param to
 * @param cmd_write usage: cmd_write [path]. if path is not specified, previous path will be used.
 * @param cmd_read usage: cmd_read [path]. if path is not specified, previous path will be used.
 */
void register_conffile_io_commands(
        if_terminal* ref,
        std::string_view cmd_load     = "load-config",  // e.g. "ld"
        std::string_view cmd_store    = "save-config",
        std::string_view initial_path = {});  // e.g. "w"

/**
 * Register option manipulation command
 *
 * @param to
 * @param cmd
 *
 * @details
 *
 *      <cmd> <config>
 *      <cmd> <config> set [values...]
 *      <cmd> <config:bool> toggle
 *      <cmd> <config:bool> detail
 *      <cmd> *<category-name>
 *
 */
void register_config_manip_command(
        if_terminal* ref,
        std::string_view cmd = "config");

/**
 * Register trace manipulation command
 *
 * @param ref
 * @param cmd
 *
 * @details
 *
 *      <cmd> <trace root> get
 *      <cmd> <trace> subscribe
 */
void register_trace_manip_command(
        if_terminal* ref,
        std::string_view cmd = "trace");

/**
 * Register logging manipulation command
 *
 * @param ref
 * @param cmd
 *
 * @details
 *
 *      <cmd> get <logger>: returns current loglevel
 *      <cmd> set [logger]: sets loglevel of given logger. set none, applies to global
 */
void register_logging_manip_command(
        if_terminal* ref,
        std::string_view cmd = "logging");

}  // namespace terminal
}  // namespace perfkit