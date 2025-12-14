#ifndef PICOCALC_LUA_H
#define PICOCALC_LUA_H

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/* Initialize Lua state and register all LOAD81 API functions */
lua_State *lua_init_load81(void);

/* Load and execute a Lua program from string */
int lua_load_program(lua_State *L, const char *code, const char *name);

/* Execute setup() function if it exists */
void lua_call_setup(lua_State *L);

/* Execute draw() function if it exists */
void lua_call_draw(lua_State *L);

/* Update keyboard state in Lua tables */
void lua_update_keyboard(lua_State *L);

/* Close Lua state */
void lua_close_load81(lua_State *L);

/* Check if there was a Lua error */
int lua_had_error(lua_State *L);

/* Get last error message */
const char *lua_get_error(lua_State *L);

#endif /* PICOCALC_LUA_H */
