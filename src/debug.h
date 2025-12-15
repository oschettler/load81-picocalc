/*
 * Debug output macros for LOAD81 on PicoCalc
 * 
 * When DEBUG_OUTPUT is defined, all debug macros will output to stdio.
 * When DEBUG_OUTPUT is not defined, debug macros become no-ops.
 */

#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG_OUTPUT
    #include <stdio.h>
    #define DEBUG_INIT() stdio_init_all()
    #define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
    #define DEBUG_INIT() ((void)0)
    #define DEBUG_PRINTF(...) ((void)0)
#endif

#endif /* DEBUG_H */
