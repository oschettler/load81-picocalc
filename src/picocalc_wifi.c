#include "picocalc_wifi.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include "debug.h"
#include <stdbool.h>
#include "picocalc_diag_server.h"

#ifdef ENABLE_9P_SERVER
/* 9P Server control functions */
extern void p9_server_request_start(void);
extern void p9_server_request_stop(void);
extern bool p9_server_is_active(void);
extern bool p9_server_is_running(void);
extern uint32_t p9_server_get_client_count(void);
#define P9_SERVER_PORT 564
#endif

static bool wifi_initialized = false;
static bool wifi_connected = false;
static char wifi_ip[16] = "0.0.0.0";

/* Initialize WiFi */
void wifi_init(void) {
    DEBUG_PRINTF("[WiFi] Initializing CYW43...\n");
    
    if (cyw43_arch_init()) {
        DEBUG_PRINTF("[WiFi] Failed to initialize CYW43\n");
        wifi_initialized = false;
        return;
    }
    
    cyw43_arch_enable_sta_mode();
    
    /* Set country code for regulatory compliance */
    cyw43_wifi_set_up(&cyw43_state, CYW43_ITF_STA, true, CYW43_COUNTRY_WORLDWIDE);
    
    DEBUG_PRINTF("[WiFi] CYW43 initialized in station mode\n");
    wifi_initialized = true;
    wifi_connected = false;
}

/* Get IP address as string */
static void update_ip_string(void) {
    if (!wifi_connected) {
        strcpy(wifi_ip, "0.0.0.0");
        return;
    }
    
    struct netif *netif = netif_default;
    if (netif) {
        ip4_addr_t *addr = netif_ip4_addr(netif);
        snprintf(wifi_ip, sizeof(wifi_ip), "%s", ip4addr_ntoa(addr));
    } else {
        strcpy(wifi_ip, "0.0.0.0");
    }
}

/* Helper function to interpret WiFi error codes */
static const char* wifi_error_string(int error) {
    switch (error) {
        case 0: return "Success";
        case -1: return "Generic error";
        case -2: return "Timeout waiting for response";
        case -3: return "Invalid parameter";
        case -4: return "Out of memory";
        case -5: return "Device busy";
        case -6: return "Device not ready";
        case -7: return "Operation timeout (30s)";
        case -8: return "Invalid state";
        case -9: return "Not supported";
        case -10: return "I/O error";
        case -11: return "Device error";
        default: return "Unknown error";
    }
}

/* Lua: wifi.connect(ssid, password) - blocking */
static int lua_wifi_connect(lua_State *L) {
    const char *ssid = luaL_checkstring(L, 1);
    const char *password = luaL_checkstring(L, 2);
    
    if (!wifi_initialized) {
        DEBUG_PRINTF("[WiFi] Not initialized\n");
        lua_pushboolean(L, 0);
        return 1;
    }
    
    DEBUG_PRINTF("[WiFi] ============ WiFi Connection Debug ============\n");
    DEBUG_PRINTF("[WiFi] SSID: '%s'\n", ssid);
    DEBUG_PRINTF("[WiFi] Password length: %d characters\n", (int)strlen(password));
    DEBUG_PRINTF("[WiFi] Auth method: WPA2-AES-PSK\n");
    DEBUG_PRINTF("[WiFi] Timeout: 30000ms (30 seconds)\n");
    
    /* Check current link status before connecting */
    int pre_link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    DEBUG_PRINTF("[WiFi] Pre-connection link status: %d\n", pre_link_status);
    
    DEBUG_PRINTF("[WiFi] Starting connection attempt...\n");
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    
    /* Blocking connect with timeout */
    int result = cyw43_arch_wifi_connect_timeout_ms(ssid, password, 
                                                     CYW43_AUTH_WPA2_AES_PSK, 
                                                     30000);
    
    uint32_t elapsed_time = to_ms_since_boot(get_absolute_time()) - start_time;
    DEBUG_PRINTF("[WiFi] Connection attempt completed in %lu ms\n", (unsigned long)elapsed_time);
    DEBUG_PRINTF("[WiFi] Result code: %d\n", result);
    DEBUG_PRINTF("[WiFi] Result meaning: %s\n", wifi_error_string(result));
    
    /* Check final link status */
    int post_link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    DEBUG_PRINTF("[WiFi] Post-connection link status: %d ", post_link_status);
    switch (post_link_status) {
        case CYW43_LINK_DOWN:
            DEBUG_PRINTF("(LINK_DOWN - not connected)\n");
            break;
        case CYW43_LINK_JOIN:
            DEBUG_PRINTF("(LINK_JOIN - joining network)\n");
            break;
        case CYW43_LINK_NOIP:
            DEBUG_PRINTF("(LINK_NOIP - connected but no IP)\n");
            break;
        case CYW43_LINK_UP:
            DEBUG_PRINTF("(LINK_UP - fully connected)\n");
            break;
        case CYW43_LINK_FAIL:
            DEBUG_PRINTF("(LINK_FAIL - connection failed)\n");
            break;
        case CYW43_LINK_NONET:
            DEBUG_PRINTF("(LINK_NONET - network not found)\n");
            break;
        case CYW43_LINK_BADAUTH:
            DEBUG_PRINTF("(LINK_BADAUTH - authentication failed)\n");
            break;
        default:
            DEBUG_PRINTF("(Unknown: %d)\n", post_link_status);
            break;
    }
    
    if (result == 0) {
        wifi_connected = true;
        update_ip_string();
        DEBUG_PRINTF("[WiFi] ✓ Successfully connected!\n");
        DEBUG_PRINTF("[WiFi] IP Address: %s\n", wifi_ip);
        
#ifdef ENABLE_9P_SERVER
        /* Start 9P server on successful WiFi connection */
        DEBUG_PRINTF("[WiFi] Starting 9P server...\n");
        p9_server_request_start();
#endif
        
        /* Start diagnostic NEX server */
        DEBUG_PRINTF("[WiFi] Starting diagnostic server on port 1900...\n");
        diag_server_init();
        if (diag_server_start()) {
            DEBUG_PRINTF("[WiFi] Diagnostic server started\n");
        } else {
            DEBUG_PRINTF("[WiFi] Failed to start diagnostic server\n");
        }
        
        DEBUG_PRINTF("[WiFi] =============================================\n");
        lua_pushboolean(L, 1);
    } else {
        wifi_connected = false;
        strcpy(wifi_ip, "0.0.0.0");
        DEBUG_PRINTF("[WiFi] ✗ Connection FAILED\n");
        
        /* Additional diagnostics */
        if (result == -7) {
            DEBUG_PRINTF("[WiFi] DIAGNOSIS: Timeout suggests one of:\n");
            DEBUG_PRINTF("[WiFi]   - Network is out of range (weak signal)\n");
            DEBUG_PRINTF("[WiFi]   - SSID is incorrect or hidden\n");
            DEBUG_PRINTF("[WiFi]   - Router not responding to connection\n");
            DEBUG_PRINTF("[WiFi]   - WiFi hardware issue\n");
        } else if (post_link_status == CYW43_LINK_BADAUTH) {
            DEBUG_PRINTF("[WiFi] DIAGNOSIS: Authentication failed\n");
            DEBUG_PRINTF("[WiFi]   - Incorrect password\n");
            DEBUG_PRINTF("[WiFi]   - Unsupported security type\n");
        } else if (post_link_status == CYW43_LINK_NONET) {
            DEBUG_PRINTF("[WiFi] DIAGNOSIS: Network not found\n");
            DEBUG_PRINTF("[WiFi]   - SSID may be incorrect\n");
            DEBUG_PRINTF("[WiFi]   - Network may be hidden\n");
            DEBUG_PRINTF("[WiFi]   - Router may be off\n");
        }
        
        DEBUG_PRINTF("[WiFi] =============================================\n");
        lua_pushboolean(L, 0);
    }
    
    return 1;
}

/* Lua: wifi.disconnect() */
static int lua_wifi_disconnect(lua_State *L) {
    if (wifi_initialized && wifi_connected) {
        DEBUG_PRINTF("[WiFi] Disconnecting...\n");
        
#ifdef ENABLE_9P_SERVER
        /* Stop 9P server on WiFi disconnect */
        DEBUG_PRINTF("[WiFi] Stopping 9P server...\n");
        p9_server_request_stop();
#endif
        
        /* Stop diagnostic server */
        if (diag_server_is_running()) {
            DEBUG_PRINTF("[WiFi] Stopping diagnostic server...\n");
            diag_server_stop();
        }
        
        cyw43_arch_disable_sta_mode();
        cyw43_arch_enable_sta_mode();
    }
    
    wifi_connected = false;
    strcpy(wifi_ip, "0.0.0.0");
    return 0;
}

/* Lua: wifi.status() */
static int lua_wifi_status(lua_State *L) {
    if (!wifi_initialized) {
        lua_pushstring(L, "not_initialized");
        return 1;
    }
    
    int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    
    switch (link_status) {
        case CYW43_LINK_DOWN:
            lua_pushstring(L, "disconnected");
            wifi_connected = false;
            break;
        case CYW43_LINK_JOIN:
            lua_pushstring(L, "connecting");
            break;
        case CYW43_LINK_NOIP:
            lua_pushstring(L, "no_ip");
            break;
        case CYW43_LINK_UP:
            lua_pushstring(L, "connected");
            wifi_connected = true;
            update_ip_string();
            break;
        case CYW43_LINK_FAIL:
            lua_pushstring(L, "failed");
            wifi_connected = false;
            break;
        case CYW43_LINK_NONET:
            lua_pushstring(L, "no_network");
            wifi_connected = false;
            break;
        case CYW43_LINK_BADAUTH:
            lua_pushstring(L, "bad_auth");
            wifi_connected = false;
            break;
        default:
            lua_pushstring(L, "unknown");
            break;
    }
    
    return 1;
}

/* Lua: wifi.ip() */
static int lua_wifi_ip(lua_State *L) {
    if (wifi_connected) {
        update_ip_string();
    }
    lua_pushstring(L, wifi_ip);
    return 1;
}

/* Lua: wifi.scan() - scan for available networks */
static int lua_wifi_scan(lua_State *L) {
    if (!wifi_initialized) {
        DEBUG_PRINTF("[WiFi] Not initialized for scan\n");
        lua_newtable(L);
        return 1;
    }
    
    DEBUG_PRINTF("[WiFi] ============ WiFi Network Scan ============\n");
    DEBUG_PRINTF("[WiFi] Starting network scan...\n");
    
    cyw43_wifi_scan_options_t scan_options = {0};
    int result = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, NULL);
    
    if (result != 0) {
        DEBUG_PRINTF("[WiFi] Scan failed with error: %d\n", result);
        lua_newtable(L);
        return 1;
    }
    
    DEBUG_PRINTF("[WiFi] Scan initiated, waiting for results...\n");
    sleep_ms(3000);  /* Wait for scan to complete */
    
    DEBUG_PRINTF("[WiFi] Scan completed\n");
    DEBUG_PRINTF("[WiFi] Note: Detailed scan results require callback implementation\n");
    DEBUG_PRINTF("[WiFi] =============================================\n");
    
    /* Return empty table for now - full implementation would need scan callback */
    lua_newtable(L);
    return 1;
}

/* Lua: wifi.debug_info() - print detailed WiFi debug information */
static int lua_wifi_debug_info(lua_State *L) {
    DEBUG_PRINTF("[WiFi] ========== WiFi Debug Information ==========\n");
    DEBUG_PRINTF("[WiFi] Initialized: %s\n", wifi_initialized ? "YES" : "NO");
    DEBUG_PRINTF("[WiFi] Connected: %s\n", wifi_connected ? "YES" : "NO");
    DEBUG_PRINTF("[WiFi] IP Address: %s\n", wifi_ip);
    
    if (wifi_initialized) {
        int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        DEBUG_PRINTF("[WiFi] Link Status: %d ", link_status);
        switch (link_status) {
            case CYW43_LINK_DOWN: DEBUG_PRINTF("(DOWN)\n"); break;
            case CYW43_LINK_JOIN: DEBUG_PRINTF("(JOIN)\n"); break;
            case CYW43_LINK_NOIP: DEBUG_PRINTF("(NOIP)\n"); break;
            case CYW43_LINK_UP: DEBUG_PRINTF("(UP)\n"); break;
            case CYW43_LINK_FAIL: DEBUG_PRINTF("(FAIL)\n"); break;
            case CYW43_LINK_NONET: DEBUG_PRINTF("(NONET)\n"); break;
            case CYW43_LINK_BADAUTH: DEBUG_PRINTF("(BADAUTH)\n"); break;
            default: DEBUG_PRINTF("(UNKNOWN)\n"); break;
        }
        
        /* Get MAC address */
        uint8_t mac[6];
        cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
        DEBUG_PRINTF("[WiFi] MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        /* Check netif status */
        struct netif *netif = netif_default;
        if (netif) {
            DEBUG_PRINTF("[WiFi] Network Interface: ACTIVE\n");
            DEBUG_PRINTF("[WiFi]   - Interface up: %s\n", netif_is_up(netif) ? "YES" : "NO");
            DEBUG_PRINTF("[WiFi]   - Link up: %s\n", netif_is_link_up(netif) ? "YES" : "NO");
        } else {
            DEBUG_PRINTF("[WiFi] Network Interface: NOT AVAILABLE\n");
        }
    }
    
    DEBUG_PRINTF("[WiFi] =============================================\n");
    return 0;
}

#ifdef ENABLE_9P_SERVER
/* Lua: wifi.p9_status() - get 9P server status */
static int lua_wifi_p9_status(lua_State *L) {
    lua_newtable(L);
    
    /* Check if server is active */
    bool active = p9_server_is_active();
    lua_pushboolean(L, active);
    lua_setfield(L, -2, "active");
    
    /* Get server state */
    bool running = p9_server_is_running();
    lua_pushboolean(L, running);
    lua_setfield(L, -2, "running");
    
    /* Get client count */
    uint32_t clients = p9_server_get_client_count();
    lua_pushinteger(L, clients);
    lua_setfield(L, -2, "clients");
    
    /* Get port */
    lua_pushinteger(L, P9_SERVER_PORT);
    lua_setfield(L, -2, "port");
    
    return 1;
}

/* Lua: wifi.p9_info() - print 9P server status (for REPL) */
static int lua_wifi_p9_info(lua_State *L) {
    bool active = p9_server_is_active();
    bool running = p9_server_is_running();
    uint32_t clients = p9_server_get_client_count();
    
    DEBUG_PRINTF("[9P] ========== 9P Server Status ==========\n");
    DEBUG_PRINTF("[9P] Active: %s\n", active ? "YES" : "NO");
    DEBUG_PRINTF("[9P] Running: %s\n", running ? "YES" : "NO");
    DEBUG_PRINTF("[9P] Clients: %lu\n", (unsigned long)clients);
    DEBUG_PRINTF("[9P] Port: %d\n", P9_SERVER_PORT);
    DEBUG_PRINTF("[9P] ========================================\n");
    
    /* Also return a formatted string for REPL display */
    char status_str[128];
    snprintf(status_str, sizeof(status_str),
             "9P: %s, Running: %s, Clients: %lu, Port: %d",
             active ? "Active" : "Inactive",
             running ? "Yes" : "No",
             (unsigned long)clients,
             P9_SERVER_PORT);
    lua_pushstring(L, status_str);
    return 1;
}
#endif

/* Get WiFi status as C string */
const char* wifi_get_status_string(void) {
    if (!wifi_initialized) {
        return "Not Init";
    }
    
    int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    
    switch (link_status) {
        case CYW43_LINK_DOWN:
            return "Disconn";
        case CYW43_LINK_JOIN:
            return "Joining";
        case CYW43_LINK_NOIP:
            return "No IP";
        case CYW43_LINK_UP:
            wifi_connected = true;
            update_ip_string();
            return "Online";
        case CYW43_LINK_FAIL:
            return "Failed";
        case CYW43_LINK_NONET:
            return "No Net";
        case CYW43_LINK_BADAUTH:
            return "Bad Auth";
        default:
            return "Unknown";
    }
}

/* Get WiFi IP as C string */
const char* wifi_get_ip_string(void) {
    if (wifi_connected) {
        update_ip_string();
    }
    return wifi_ip;
}

/* Register WiFi Lua API */
void wifi_register_lua(lua_State *L) {
    /* Create wifi table */
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_wifi_connect);
    lua_setfield(L, -2, "connect");
    
    lua_pushcfunction(L, lua_wifi_disconnect);
    lua_setfield(L, -2, "disconnect");
    
    lua_pushcfunction(L, lua_wifi_status);
    lua_setfield(L, -2, "status");
    
    lua_pushcfunction(L, lua_wifi_ip);
    lua_setfield(L, -2, "ip");
    
    lua_pushcfunction(L, lua_wifi_scan);
    lua_setfield(L, -2, "scan");
    
    lua_pushcfunction(L, lua_wifi_debug_info);
    lua_setfield(L, -2, "debug_info");
    
#ifdef ENABLE_9P_SERVER
    lua_pushcfunction(L, lua_wifi_p9_status);
    lua_setfield(L, -2, "p9_status");
    
    lua_pushcfunction(L, lua_wifi_p9_info);
    lua_setfield(L, -2, "p9_info");
#endif
    
    lua_setglobal(L, "wifi");
}
