#ifndef PICOCALC_FILE_SERVER_H
#define PICOCALC_FILE_SERVER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @file picocalc_file_server.h
 * @brief LOAD81R File Server - Remote shell server for PicoCalc
 * 
 * Provides TCP-based file system access and Lua REPL functionality.
 * Replaces the diagnostic server on port 1900.
 * 
 * Protocol: Text-based command/response
 * Commands: HELLO, PWD, CD, LS, CAT, PUT, MKDIR, RM, STAT, REPL, PING, QUIT
 */

/* Server configuration */
#define FILE_SERVER_PORT 1900
#define FILE_SERVER_MAX_CLIENTS 1
#define FILE_SERVER_CMD_BUFFER_SIZE 1024
#define FILE_SERVER_RESPONSE_BUFFER_SIZE 4096
#define FILE_SERVER_FILE_BUFFER_SIZE 8192
#define FILE_SERVER_MAX_FILE_SIZE (1024 * 1024)  /* 1MB */

/* Protocol version */
#define FILE_SERVER_PROTOCOL_VERSION "load81r/1.0"

/**
 * Initialize file server subsystem
 * Must be called before file_server_start()
 * 
 * @return true on success, false on failure
 */
bool file_server_init(void);

/**
 * Start the file server
 * Begins listening on FILE_SERVER_PORT
 * 
 * @return true on success, false on failure
 */
bool file_server_start(void);

/**
 * Stop the file server
 * Closes all connections and stops listening
 */
void file_server_stop(void);

/**
 * Check if file server is running
 * 
 * @return true if server is active, false otherwise
 */
bool file_server_is_running(void);

/**
 * Get server statistics
 * 
 * @param total_connections Output: total connections since start
 * @param total_requests Output: total requests processed
 * @param active_clients Output: currently connected clients
 */
void file_server_get_stats(uint32_t *total_connections, 
                           uint32_t *total_requests,
                           uint32_t *active_clients);

#endif /* PICOCALC_FILE_SERVER_H */