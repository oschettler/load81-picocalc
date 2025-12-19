#ifndef PICOCALC_REPL_HANDLER_H
#define PICOCALC_REPL_HANDLER_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @file picocalc_repl_handler.h
 * @brief Lua REPL handler for LOAD81R server
 * 
 * Provides remote Lua code execution with output capture.
 * Uses inter-core communication to execute code on Core 0.
 */

/* Error codes */
typedef enum {
    REPL_OK = 0,
    REPL_ERR_TIMEOUT,
    REPL_ERR_SYNTAX,
    REPL_ERR_RUNTIME,
    REPL_ERR_NO_MEMORY,
    REPL_ERR_BUSY
} repl_error_t;

/**
 * Initialize REPL handler
 * Sets up inter-core communication
 * 
 * @return REPL_OK on success, error code otherwise
 */
repl_error_t repl_init(void);

/**
 * Execute Lua code and capture output
 * Sends code to Core 0 for execution via FIFO
 * Waits for response with timeout
 * 
 * @param code Lua code to execute
 * @param output Output: allocated string with result (caller must free)
 * @return REPL_OK on success, error code otherwise
 */
repl_error_t repl_execute(const char *code, char **output);

/**
 * Check if REPL is available
 * 
 * @return true if REPL can accept requests, false if busy
 */
bool repl_is_available(void);

/**
 * Get error message string
 * 
 * @param error Error code
 * @return Human-readable error message
 */
const char *repl_error_string(repl_error_t error);

#endif /* PICOCALC_REPL_HANDLER_H */