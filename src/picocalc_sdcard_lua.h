#ifndef PICOCALC_SDCARD_LUA_H
#define PICOCALC_SDCARD_LUA_H

#include "lua.h"

/* Register SD card debugging functions with Lua */
void sdcard_register_lua(lua_State *L);

#endif /* PICOCALC_SDCARD_LUA_H */
