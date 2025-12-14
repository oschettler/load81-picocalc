#ifndef PICOCALC_REPL_H
#define PICOCALC_REPL_H

#include "lua.h"

/* Run interactive Lua REPL over serial */
void repl_run(lua_State *L);

#endif /* PICOCALC_REPL_H */
