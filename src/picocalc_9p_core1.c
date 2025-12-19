/**
 * @file picocalc_9p_core1.c
 * @brief Core 1 Entry Point for 9P Server
 * 
 * This file contains the Core 1 initialization and main loop
 * for the 9P filesystem server, running independently from the
 * Lua interpreter on Core 0.
 */

#ifdef ENABLE_9P_SERVER

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "picocalc_9p.h"
#include "picocalc_fat32_sync.h"
#include "picocalc_mdns.h"
#include "debug.h"

/* Core 1 state */
static volatile bool core1_running = false;
static volatile bool server_should_run = false;

/**
 * @brief Core 1 main loop
 * 
 * Runs the 9P server and mDNS responder on Core 1,
 * independent of the Lua interpreter on Core 0.
 */
static void core1_entry(void) {
    DEBUG_PRINTF("[Core1] Starting 9P server core...\n");
    
    /* Initialize FAT32 synchronization */
    fat32_sync_init();
    DEBUG_PRINTF("[Core1] FAT32 sync initialized\n");
    
    /* Initialize 9P server */
    if (!p9_server_init()) {
        DEBUG_PRINTF("[Core1] Failed to initialize 9P server\n");
        return;
    }
    DEBUG_PRINTF("[Core1] 9P server initialized\n");
    
    core1_running = true;
    
    /* Main loop */
    while (core1_running) {
        /* Check if server should be running */
        if (server_should_run && !p9_server_is_running()) {
            /* Start server */
            DEBUG_PRINTF("[Core1] Starting 9P server on port %d...\n", P9_SERVER_PORT);
            if (p9_server_start()) {
                DEBUG_PRINTF("[Core1] 9P server started successfully\n");
                
                /* Start mDNS */
                if (mdns_init(P9_MDNS_SERVICE_NAME, "PicoCalc 9P Server", P9_SERVER_PORT)) {
                    if (mdns_start()) {
                        DEBUG_PRINTF("[Core1] mDNS responder started\n");
                    } else {
                        DEBUG_PRINTF("[Core1] Failed to start mDNS responder\n");
                    }
                }
            } else {
                DEBUG_PRINTF("[Core1] Failed to start 9P server\n");
            }
        } else if (!server_should_run && p9_server_is_running()) {
            /* Stop server */
            DEBUG_PRINTF("[Core1] Stopping 9P server...\n");
            mdns_stop();
            p9_server_stop();
            DEBUG_PRINTF("[Core1] 9P server stopped\n");
        }
        
        /* Poll lwIP network stack - CRITICAL for incoming connections! */
        cyw43_arch_poll();
        
        /* Poll server and mDNS */
        if (p9_server_is_running()) {
            p9_server_poll();
            mdns_poll();
        }
        
        /* NO sleep_ms() here - we need maximum responsiveness for network events!
         * cyw43_arch_poll() already includes appropriate internal delays */
    }
    
    /* Cleanup */
    if (p9_server_is_running()) {
        mdns_stop();
        p9_server_stop();
    }
    
    DEBUG_PRINTF("[Core1] 9P server core stopped\n");
}

/**
 * @brief Launch Core 1 with 9P server
 * 
 * Should be called during system initialization (from Core 0).
 */
void p9_core1_launch(void) {
    DEBUG_PRINTF("[Core1] Launching Core 1...\n");
    multicore_launch_core1(core1_entry);
    
    /* Wait for Core 1 to initialize */
    while (!core1_running) {
        sleep_ms(10);
    }
    
    DEBUG_PRINTF("[Core1] Core 1 launched successfully\n");
}

/**
 * @brief Start 9P server (called when WiFi connects)
 * 
 * This function is called from Core 0 when WiFi connection
 * is established. It signals Core 1 to start the server.
 */
void p9_server_request_start(void) {
    DEBUG_PRINTF("[Core1] Server start requested\n");
    server_should_run = true;
}

/**
 * @brief Stop 9P server (called when WiFi disconnects)
 * 
 * This function is called from Core 0 when WiFi connection
 * is lost. It signals Core 1 to stop the server.
 */
void p9_server_request_stop(void) {
    DEBUG_PRINTF("[Core1] Server stop requested\n");
    server_should_run = false;
}

/**
 * @brief Check if 9P server is running
 * 
 * @return true if server is active
 */
bool p9_server_is_active(void) {
    return server_should_run && p9_server_is_running();
}

/**
 * @brief Get server statistics
 * 
 * @param stats Pointer to statistics structure to populate
 */
void p9_server_get_statistics(p9_server_stats_t *stats) {
    if (p9_server_is_running()) {
        p9_server_get_stats(stats);
    } else {
        memset(stats, 0, sizeof(p9_server_stats_t));
    }
}

#endif /* ENABLE_9P_SERVER */