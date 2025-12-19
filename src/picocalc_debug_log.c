/**
 * @file picocalc_debug_log.c
 * @brief Thread-safe debug logging system
 * 
 * Provides a circular buffer for debug messages that can be accessed
 * via the diagnostic server. Thread-safe for use from both cores.
 */

#include "picocalc_debug_log.h"
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define DEBUG_LOG_SIZE 8192  /* 8KB circular buffer */

static struct {
    char buffer[DEBUG_LOG_SIZE];
    uint32_t write_pos;
    uint32_t total_bytes;
    mutex_t mutex;
    bool initialized;
} g_debug_log;

void debug_log_init(void) {
    memset(&g_debug_log, 0, sizeof(g_debug_log));
    mutex_init(&g_debug_log.mutex);
    g_debug_log.initialized = true;
}

void debug_log(const char *format, ...) {
    if (!g_debug_log.initialized) {
        return;
    }
    
    char temp[256];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    
    if (len <= 0) {
        return;
    }
    
    /* Ensure newline at end */
    if (len < (int)sizeof(temp) - 1 && temp[len-1] != '\n') {
        temp[len++] = '\n';
        temp[len] = '\0';
    }
    
    /* NON-BLOCKING write to circular buffer - safe for interrupt context!
     * If mutex is busy, skip this log entry rather than blocking/crashing */
    if (!mutex_try_enter(&g_debug_log.mutex, NULL)) {
        return;  /* Mutex busy, skip this log entry */
    }
    
    for (int i = 0; i < len; i++) {
        g_debug_log.buffer[g_debug_log.write_pos] = temp[i];
        g_debug_log.write_pos = (g_debug_log.write_pos + 1) % DEBUG_LOG_SIZE;
        g_debug_log.total_bytes++;
    }
    
    mutex_exit(&g_debug_log.mutex);
}

const char* debug_log_get(uint32_t *out_len) {
    if (!g_debug_log.initialized) {
        *out_len = 0;
        return "";
    }
    
    /* NON-BLOCKING read - safe for interrupt context */
    if (!mutex_try_enter(&g_debug_log.mutex, NULL)) {
        *out_len = 0;
        return "";  /* Mutex busy, return empty */
    }
    
    /* If buffer hasn't wrapped, return from start */
    if (g_debug_log.total_bytes < DEBUG_LOG_SIZE) {
        *out_len = g_debug_log.write_pos;
    } else {
        /* Buffer has wrapped - return full buffer */
        *out_len = DEBUG_LOG_SIZE;
    }
    
    mutex_exit(&g_debug_log.mutex);
    
    return g_debug_log.buffer;
}

void debug_log_clear(void) {
    if (!g_debug_log.initialized) {
        return;
    }
    
    mutex_enter_blocking(&g_debug_log.mutex);
    memset(g_debug_log.buffer, 0, DEBUG_LOG_SIZE);
    g_debug_log.write_pos = 0;
    g_debug_log.total_bytes = 0;
    mutex_exit(&g_debug_log.mutex);
}