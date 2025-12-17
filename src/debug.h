/*
 * Debug output macros for LOAD81 on PicoCalc
 *
 * When DEBUG_OUTPUT is defined, all debug macros will output to stdio
 * only if a USB cable is connected. This prevents the device from hanging
 * when no cable is present.
 *
 * When DEBUG_OUTPUT is not defined, debug macros become no-ops.
 */

#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG_OUTPUT
    #include <stdio.h>
    #include "pico/stdlib.h"
    
    /*
     * Initialize stdio for USB output.
     * Uses a fixed 2-second delay to allow USB enumeration.
     * USB CDC is non-blocking, so this won't hang even without a cable.
     */
    static inline void debug_init_usb(void) {
        stdio_init_all();
        /* Fixed delay for USB enumeration - USB CDC won't block without cable */
        sleep_ms(2000);
    }
    
    #define DEBUG_INIT() debug_init_usb()
    
    /*
     * Print to stdio with explicit flush.
     * USB CDC will buffer and send when host is connected,
     * or discard if no host is present (non-blocking).
     */
    #define DEBUG_PRINTF(...) do { \
        printf(__VA_ARGS__); \
        fflush(stdout); \
    } while(0)
#else
    #define DEBUG_INIT() ((void)0)
    #define DEBUG_PRINTF(...) ((void)0)
#endif

#endif /* DEBUG_H */
