#ifndef PICOCALC_DEBUG_LOG_H
#define PICOCALC_DEBUG_LOG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize debug log buffer
 */
void debug_log_init(void);

/**
 * @brief Add a message to the debug log
 * 
 * Thread-safe function to add debug messages to a circular buffer.
 * Messages are timestamped and can be retrieved via diagnostic server.
 * 
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void debug_log(const char *format, ...);

/**
 * @brief Get debug log contents
 * 
 * Returns pointer to the debug log buffer for reading.
 * Buffer contains newline-separated log entries.
 * 
 * @param out_len Pointer to receive buffer length
 * @return Pointer to log buffer (read-only)
 */
const char* debug_log_get(uint32_t *out_len);

/**
 * @brief Clear debug log
 */
void debug_log_clear(void);

#endif /* PICOCALC_DEBUG_LOG_H */