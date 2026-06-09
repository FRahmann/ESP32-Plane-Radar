#pragma once

namespace services::config_server {

/**
 * Start the station-mode configuration web server (and station mDNS) once
 * connected. Idempotent — safe to call repeatedly; only the first call starts
 * the server. Reachable at http://plane-radar.local or the device IP.
 */
void begin();

/** Service pending HTTP clients. Call from the main loop. No-op until begun. */
void loop();

}  // namespace services::config_server
