#include "picocalc_nex.h"
#include <string.h>
#include <stdlib.h>

/* Initialize NEX */
void nex_init(void) {
    /* NEX protocol initialization */
}

/* Lua: nex.load(url) */
static int lua_nex_load(lua_State *L) {
    /* Simplified NEX implementation - would need full TCP client */
    /* For now, return error */
    lua_pushnil(L);
    lua_pushstring(L, "NEX protocol not yet fully implemented");
    return 2;
}

/* Lua: nex.parse(content) */
static int lua_nex_parse(lua_State *L) {
    size_t len;
    const char *content = lua_tolstring(L, 1, &len);
    if (!content) {
        lua_newtable(L);
        return 1;
    }
    
    /* Parse NEX/Gemtext format */
    /* Return structured table */
    lua_newtable(L);
    
    /* Simple line-by-line parsing */
    char *dup = strdup(content);
    char *line = strtok(dup, "\n");
    int index = 1;
    
    while (line) {
        lua_newtable(L);
        
        /* Check line type */
        if (strncmp(line, "=>", 2) == 0) {
            /* Link line */
            lua_pushstring(L, "link");
            lua_setfield(L, -2, "type");
            lua_pushstring(L, line + 2);
            lua_setfield(L, -2, "text");
        } else if (strncmp(line, "#", 1) == 0) {
            /* Heading */
            lua_pushstring(L, "heading");
            lua_setfield(L, -2, "type");
            lua_pushstring(L, line + 1);
            lua_setfield(L, -2, "text");
        } else {
            /* Text line */
            lua_pushstring(L, "text");
            lua_setfield(L, -2, "type");
            lua_pushstring(L, line);
            lua_setfield(L, -2, "text");
        }
        
        lua_rawseti(L, -2, index++);
        line = strtok(NULL, "\n");
    }
    
    free(dup);
    return 1;
}

/* Register NEX Lua API */
void nex_register_lua(lua_State *L) {
    /* Create nex table */
    lua_newtable(L);
    
    lua_pushcfunction(L, lua_nex_load);
    lua_setfield(L, -2, "load");
    
    lua_pushcfunction(L, lua_nex_parse);
    lua_setfield(L, -2, "parse");
    
    lua_setglobal(L, "nex");
}
