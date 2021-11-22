#pragma once
#include "perfkit/terminal.h"

namespace perfkit::terminal::net {
/**
 * Create net client session instance
 *
 * @param info
 */
terminal_ptr create(struct terminal_init_info const& info);

/**
 * Create net client from configuration profile
 * @param config_profile_name
 */
terminal_ptr create(std::string config_profile_name);

enum class operation_mode {
  invalid,
  independent_server,
  relay_server_provider,
};

struct terminal_init_info {
  /** Session name to introduce self */
  std::string const name;

  /** Describes this session */
  std::string description;

  /** Host name to connect with. */
  std::string host_name = "localhost";

  /** Host port to connect with. */
  uint16_t host_port = 25572;

  /**
   * Specify if remote connection should be read-only.
   * For security reason, default value is set to true.
   * if read_only_access is enabled, all mutation operations will silently be discarded,
   *  including shell commands. */
  bool read_only_access = true;

  /**
   * Authentication information. If empty, every login attempt will be permitted. */
  std::string id, password;

  /**
   * Specify how many milliseconds to wait for host response */
  std::chrono::milliseconds connection_timeout_ms{5'000};

  /**
   * Create terminal instance as an independent server.
   * Multiple clients will directly connect to this instance, which may cause
   *  performance problem when they're too many.
   *
   * @param bindaddr
   * @param port
   */
  void serve(std::string bindaddr, uint16_t port) {
    _string_param = std::move(bindaddr);
    _port_param   = port;
  }

  void serve(uint16_t port) { serve("0.0.0.0", port); }

  /**
   * Create terminal instance as an relay-server provider.
   * Metadata of this session will be registered to relay server, and only one tcp socket
   *  connection is maintained while up. Clients will indirectly communicate with this
   *  net instance through relay server.
   */
  void relay_to(std::string addr, uint16_t port) {
    // TODO.
  }

 private:
  friend class dispatcher;  // internal use
  operation_mode _mode = operation_mode::invalid;
  std::string _string_param;
  uint16_t _port_param = 0;

 public:
  explicit terminal_init_info(std::string session_name) : name{std::move(session_name)} {};
};

}  // namespace perfkit::terminal::net
