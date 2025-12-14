#include "picocalc_wifi.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

static bool wifi_connected = false;
static char wifi_ip[16] = "0.0.0.0";

/* Initialize WiFi */
void wifi_init(void) {
    /* WiFi initialization - simplified for now */
    wifi_connected = false;
}

/* Lua: wifi.connect(ssid, password) */
static int lua_wifi_connect(lua_State *L) {
    /* WiFi not fully implemented yet */
    wifi_connected = false;
    lua_pushboolean(L, 0);
    return 1;
}

/* Lua: wifi.disconnect() */
static int lua_wifi_disconnect(lua_State *L) {
    wifi_connected = false;
    strcpy(wifi_ip, "0.0.0.0");
    return 0;
}

/* Lua: wifi.status() */
static int lua_wifi_status(lua_State *L) {
    lua_pushstring(L, "not_implemented");
    return 1;
}

/* Lua: wifi.ip() */
static int lua_wifi_ip(lua_State *L) {
    lua_pushstring(L, wifi_ip);
    return 1;
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
    
    lua_setglobal(L, "wifi");
}
