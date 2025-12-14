#ifndef PICOCALC_NEX_H
#define PICOCALC_NEX_H

#include <lua.h>

/* Initialize NEX protocol subsystem */
void nex_init(void);

/* Register NEX Lua bindings */
void nex_register_lua(lua_State *L);

/* NEX Lua API:
 * nex.load(url) - Load NEX page, returns content, content_type
 * nex.parse(content) - Parse NEX content into structured format
 */

#endif /* PICOCALC_NEX_H */
