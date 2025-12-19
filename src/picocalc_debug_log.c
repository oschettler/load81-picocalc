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
    
    /* BLOCKING write to circular buffer with timeout
     * Use a timeout to avoid deadlock but ensure messages get written */
    absolute_time_t timeout = make_timeout_time_ms(100);  /* 100ms timeout */
    if (!mutex_enter_block_until(&g_debug_log.mutex, &timeout)) {
        return;  /* Timeout - skip this log entry */
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
        mutex_exit(&g_debug_log.mutex);
        return g_debug_log.buffer;
    }
    
    /* Buffer has wrapped - need to return in chronological order
     * Most recent data is at write_pos, oldest is at write_pos+1
     * We need to rearrange: [write_pos..end] + [0..write_pos-1]
     * Use a static buffer to avoid dynamic allocation */
    static char ordered_buffer[DEBUG_LOG_SIZE];
    uint32_t first_part_len = DEBUG_LOG_SIZE - g_debug_log.write_pos;
    uint32_t second_part_len = g_debug_log.write_pos;
    
    /* Copy oldest messages first (from write_pos to end) */
    memcpy(ordered_buffer, g_debug_log.buffer + g_debug_log.write_pos, first_part_len);
    /* Then copy newer messages (from start to write_pos) */
    memcpy(ordered_buffer + first_part_len, g_debug_log.buffer, second_part_len);
    
    *out_len = DEBUG_LOG_SIZE;
    mutex_exit(&g_debug_log.mutex);
    
    return ordered_buffer;
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