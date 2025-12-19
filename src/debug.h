/*
 * Debug output macros for LOAD81 on PicoCalc
 *
 * When DEBUG_OUTPUT is defined, debug output goes to the debug log buffer
 * which can be accessed via the diagnostic server or optionally to LCD.
 *
 * When DEBUG_OUTPUT is not defined, debug macros become no-ops.
 */

#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG_OUTPUT
    #include <stdio.h>
    #include <stdarg.h>
    #include "pico/stdlib.h"
    #include "picocalc_debug_log.h"
    
    /*
     * Initialize debug system.
     * Debug output goes to internal buffer accessible via diagnostic server.
     */
    static inline void debug_init_system(void) {
        /* Debug log is initialized in main.c via debug_log_init() */
    }
    
    #define DEBUG_INIT() debug_init_system()
    
    /*
     * Print to debug log buffer.
     * Messages are stored in circular buffer and can be retrieved
     * via diagnostic server on port 1901.
     */
    #define DEBUG_PRINTF(...) debug_log(__VA_ARGS__)
#else
    #define DEBUG_INIT() ((void)0)
    #define DEBUG_PRINTF(...) ((void)0)
#endif

#endif /* DEBUG_H */
