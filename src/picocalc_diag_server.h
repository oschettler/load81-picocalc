#ifndef PICOCALC_DIAG_SERVER_H
#define PICOCALC_DIAG_SERVER_H

#include <stdbool.h>

/**
 * @brief Initialize diagnostic HTTP server
 * 
 * Creates a simple HTTP server on port 8080 that responds with
 * system status including 9P server state, WiFi info, and connection stats.
 * 
 * @return true if initialization successful
 */
bool diag_server_init(void);

/**
 * @brief Start diagnostic server
 * 
 * Starts listening on port 8080 for HTTP requests.
 * Should be called after WiFi connection is established.
 * 
 * @return true if server started successfully
 */
bool diag_server_start(void);

/**
 * @brief Stop diagnostic server
 */
void diag_server_stop(void);

/**
 * @brief Check if diagnostic server is running
 * 
 * @return true if server is active and listening
 */
bool diag_server_is_running(void);

#endif /* PICOCALC_DIAG_SERVER_H */