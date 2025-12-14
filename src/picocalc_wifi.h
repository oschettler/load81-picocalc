#ifndef PICOCALC_WIFI_H
#define PICOCALC_WIFI_H

#include <lua.h>

/* Initialize WiFi subsystem */
void wifi_init(void);

/* Get WiFi status as C string (for display) */
const char* wifi_get_status_string(void);

/* Get WiFi IP as C string (for display) */
const char* wifi_get_ip_string(void);

/* Register WiFi Lua bindings */
void wifi_register_lua(lua_State *L);

/* WiFi Lua API:
 * wifi.connect(ssid, password) - Connect to WiFi network
 * wifi.disconnect() - Disconnect from WiFi
 * wifi.status() - Get connection status ("connected", "disconnected", "connecting")
 * wifi.ip() - Get IP address string
 */

#endif /* PICOCALC_WIFI_H */
