#pragma once
#include "perfkit/terminal.h"

namespace perfkit::terminal::net_provider {
/**
 * Create net client session instance
 *
 * @param info
 * @return
 */
terminal_ptr create(std::string session_name, struct init_info const& info);

struct init_info {
  /** Host name to connect with. */
  std::string host_name = "localhost";

  /** Host port to connect with. */
  uint16_t host_port = 25572;

  /** Polling does not send actual data, but only notifies latest successful fetch timestamp */
  perfkit::milliseconds poll_interval{200};

  /**
   * Specify if remote connection should be read-only.
   * For security reason, default value is set to true. */
  bool read_only_access = true;

  /**
   * Specify if password is required for access.
   * If password is empty, any access will be allowed. */
  std::string password = "";
};

}  // namespace perfkit::terminal::net_provider