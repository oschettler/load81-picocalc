#include "picocalc_lua.h"
#include "picocalc_graphics.h"
#include "picocalc_framebuffer.h"
#include "picocalc_keyboard.h"
#include "picocalc_editor.h"
#include "picocalc_wifi.h"
#include "fat32.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int lua_error_flag = 0;
static char lua_error_msg[512] = "";

/* Helper: Set Lua table field to number */
static void set_table_field_number(lua_State *L, const char *table, const char *field, lua_Number value) {
    lua_getglobal(L, table);
    if (lua_istable(L, -1)) {
        lua_pushstring(L, field);
        lua_pushnumber(L, value);
        lua_settable(L, -3);
    }
    lua_pop(L, 1);
}

/* Helper: Set Lua table field to string */
static void set_table_field_string(lua_State *L, const char *table, const char *field, const char *value) {
    lua_getglobal(L, table);
    if (lua_istable(L, -1)) {
        lua_pushstring(L, field);
        lua_pushstring(L, value);
        lua_settable(L, -3);
    }
    lua_pop(L, 1);
}

/* Lua binding: fill(r, g, b, alpha) */
static int lua_fill(lua_State *L) {
    g_draw_r = (int)lua_tonumber(L, 1);
    g_draw_g = (int)lua_tonumber(L, 2);
    g_draw_b = (int)lua_tonumber(L, 3);
    g_draw_alpha = (int)(lua_tonumber(L, 4) * 255.0);
    
    /* Clamp values */
    if (g_draw_r < 0) g_draw_r = 0; if (g_draw_r > 255) g_draw_r = 255;
    if (g_draw_g < 0) g_draw_g = 0; if (g_draw_g > 255) g_draw_g = 255;
    if (g_draw_b < 0) g_draw_b = 0; if (g_draw_b > 255) g_draw_b = 255;
    if (g_draw_alpha < 0) g_draw_alpha = 0; if (g_draw_alpha > 255) g_draw_alpha = 255;
    
    return 0;
}

/* Lua binding: background(r, g, b) */
static int lua_background(lua_State *L) {
    int r = (int)lua_tonumber(L, 1);
    int g = (int)lua_tonumber(L, 2);
    int b = (int)lua_tonumber(L, 3);
    fb_fill_background(r, g, b);
    return 0;
}

/* Lua binding: rect(x, y, width, height) */
static int lua_rect(lua_State *L) {
    int x = (int)lua_tonumber(L, 1);
    int y = (int)lua_tonumber(L, 2);
    int w = (int)lua_tonumber(L, 3);
    int h = (int)lua_tonumber(L, 4);
    gfx_draw_box(x, y, x + w - 1, y + h - 1);
    return 0;
}

/* Lua binding: ellipse(x, y, rx, ry) */
static int lua_ellipse(lua_State *L) {
    int x = (int)lua_tonumber(L, 1);
    int y = (int)lua_tonumber(L, 2);
    int rx = (int)lua_tonumber(L, 3);
    int ry = (int)lua_tonumber(L, 4);
    gfx_draw_ellipse(x, y, rx, ry);
    return 0;
}

/* Lua binding: line(x1, y1, x2, y2) */
static int lua_line(lua_State *L) {
    int x1 = (int)lua_tonumber(L, 1);
    int y1 = (int)lua_tonumber(L, 2);
    int x2 = (int)lua_tonumber(L, 3);
    int y2 = (int)lua_tonumber(L, 4);
    gfx_draw_line(x1, y1, x2, y2);
    return 0;
}

/* Lua binding: triangle(x1, y1, x2, y2, x3, y3) */
static int lua_triangle(lua_State *L) {
    int x1 = (int)lua_tonumber(L, 1);
    int y1 = (int)lua_tonumber(L, 2);
    int x2 = (int)lua_tonumber(L, 3);
    int y2 = (int)lua_tonumber(L, 4);
    int x3 = (int)lua_tonumber(L, 5);
    int y3 = (int)lua_tonumber(L, 6);
    gfx_draw_triangle(x1, y1, x2, y2, x3, y3);
    return 0;
}

/* Lua binding: text(x, y, string) */
static int lua_text(lua_State *L) {
    int x = (int)lua_tonumber(L, 1);
    int y = (int)lua_tonumber(L, 2);
    size_t len;
    const char *str = lua_tolstring(L, 3, &len);
    if (str) {
        gfx_draw_string(x, y, str, (int)len);
    }
    return 0;
}

/* Lua binding: getpixel(x, y) - returns r, g, b */
static int lua_getpixel(lua_State *L) {
    int x = (int)lua_tonumber(L, 1);
    int y = (int)lua_tonumber(L, 2);
    int r, g, b;
    fb_get_pixel(x, y, &r, &g, &b);
    lua_pushnumber(L, r);
    lua_pushnumber(L, g);
    lua_pushnumber(L, b);
    return 3;
}

/* Lua binding: setFPS(fps) */
static int lua_setFPS(lua_State *L) {
    /* Note: FPS control is handled in main loop */
    return 0;
}

/* Custom print() function that outputs to serial console when DEBUG_OUTPUT is enabled */
static int lua_print(lua_State *L) {
    int n = lua_gettop(L);  /* number of arguments */
    
    #ifdef DEBUG_OUTPUT
    /* Output to serial console if DEBUG_OUTPUT is enabled */
    lua_getglobal(L, "tostring");
    for (int i = 1; i <= n; i++) {
        const char *s;
        size_t l;
        lua_pushvalue(L, -1);     /* function to be called */
        lua_pushvalue(L, i);      /* value to print */
        lua_call(L, 1, 1);
        s = lua_tolstring(L, -1, &l);  /* get result */
        if (s == NULL)
            return luaL_error(L, "'tostring' must return a string to 'print'");
        if (i > 1) printf("\t");
        printf("%s", s);
        lua_pop(L, 1);  /* pop result */
    }
    printf("\n");
    fflush(stdout);
    #else
    /* When DEBUG_OUTPUT is not enabled, print() becomes a no-op */
    /* This prevents crashes when stdout is not available */
    (void)n;  /* Suppress unused variable warning */
    #endif
    
    return 0;
}

/* Lua binding: edit(filename) */
static int lua_edit(lua_State *L) {
    const char *filename = lua_tostring(L, 1);
    if (!filename) {
        return luaL_error(L, "edit() requires a filename string");
    }
    
    /* Initialize editor before use */
    editor_init();
    
    /* Run the editor */
    int result = editor_run(filename);
    
    /* Return result: 0 = saved and exit, 1 = error */
    lua_pushnumber(L, result);
    return 1;
}

/* Lua binding: mkdir(path) - create directory recursively */
static int lua_mkdir(lua_State *L) {
    const char *path = lua_tostring(L, 1);
    if (!path) {
        return luaL_error(L, "mkdir() requires a path string");
    }
    
    /* Skip leading slash if present */
    if (path[0] == '/') {
        path++;
    }
    
    /* If path is empty after removing slash, nothing to do */
    if (path[0] == '\0') {
        lua_pushboolean(L, 1);
        return 1;
    }
    
    /* Build path incrementally, creating each directory level */
    char current_path[256] = "/";
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    char *token = strtok(path_copy, "/");
    while (token != NULL) {
        /* Append next directory component */
        if (strlen(current_path) > 1) {
            strncat(current_path, "/", sizeof(current_path) - strlen(current_path) - 1);
        }
        strncat(current_path, token, sizeof(current_path) - strlen(current_path) - 1);
        
        /* Try to open the directory to see if it exists */
        fat32_file_t test_dir;
        fat32_error_t result = fat32_open(&test_dir, current_path);
        
        if (result == FAT32_OK) {
            /* Directory exists, close it and continue */
            fat32_close(&test_dir);
        } else if (result == FAT32_ERROR_FILE_NOT_FOUND || result == FAT32_ERROR_DIR_NOT_FOUND) {
            /* Directory doesn't exist, create it */
            fat32_file_t parent_dir;
            
            /* Find parent path */
            char parent_path[256];
            strncpy(parent_path, current_path, sizeof(parent_path) - 1);
            parent_path[sizeof(parent_path) - 1] = '\0';
            char *last_slash = strrchr(parent_path, '/');
            if (last_slash && last_slash != parent_path) {
                *last_slash = '\0';
            } else {
                strcpy(parent_path, "/");
            }
            
            /* Open parent directory */
            result = fat32_open(&parent_dir, parent_path);
            if (result != FAT32_OK) {
                lua_pushboolean(L, 0);
                lua_pushstring(L, fat32_error_string(result));
                return 2;
            }
            
            /* Create the directory */
            result = fat32_dir_create(&parent_dir, current_path);
            fat32_close(&parent_dir);
            
            if (result != FAT32_OK) {
                lua_pushboolean(L, 0);
                lua_pushstring(L, fat32_error_string(result));
                return 2;
            }
        } else {
            /* Some other error occurred */
            lua_pushboolean(L, 0);
            lua_pushstring(L, fat32_error_string(result));
            return 2;
        }
        
        token = strtok(NULL, "/");
    }
    
    lua_pushboolean(L, 1);
    return 1;
}

/* Initialize Lua and register LOAD81 API */
lua_State *lua_init_load81(void) {
    lua_State *L = luaL_newstate();
    if (!L) return NULL;
    
    /* Load standard libraries */
    luaL_openlibs(L);
    
    /* Set WIDTH and HEIGHT constants */
    lua_pushnumber(L, FB_WIDTH);
    lua_setglobal(L, "WIDTH");
    lua_pushnumber(L, FB_HEIGHT);
    lua_setglobal(L, "HEIGHT");
    
    /* Initialize keyboard and mouse tables */
    lua_newtable(L);
    lua_setglobal(L, "keyboard");
    
    lua_newtable(L);
    lua_setglobal(L, "mouse");
    
    /* Create keyboard.pressed table */
    lua_getglobal(L, "keyboard");
    lua_newtable(L);
    lua_setfield(L, -2, "pressed");
    lua_pop(L, 1);
    
    /* Initialize keyboard state fields */
    set_table_field_string(L, "keyboard", "state", "none");
    set_table_field_string(L, "keyboard", "key", "");
    
    /* Mouse not supported but initialize anyway */
    set_table_field_number(L, "mouse", "x", 0);
    set_table_field_number(L, "mouse", "y", 0);
    
    /* Register drawing functions */
    lua_pushcfunction(L, lua_fill);
    lua_setglobal(L, "fill");
    
    lua_pushcfunction(L, lua_background);
    lua_setglobal(L, "background");
    
    lua_pushcfunction(L, lua_rect);
    lua_setglobal(L, "rect");
    
    lua_pushcfunction(L, lua_ellipse);
    lua_setglobal(L, "ellipse");
    
    lua_pushcfunction(L, lua_line);
    lua_setglobal(L, "line");
    
    lua_pushcfunction(L, lua_triangle);
    lua_setglobal(L, "triangle");
    
    lua_pushcfunction(L, lua_text);
    lua_setglobal(L, "text");
    
    lua_pushcfunction(L, lua_getpixel);
    lua_setglobal(L, "getpixel");
    
    lua_pushcfunction(L, lua_setFPS);
    lua_setglobal(L, "setFPS");
    
    /* Register custom print() function to override standard library */
    lua_pushcfunction(L, lua_print);
    lua_setglobal(L, "print");
    
    /* Register editor function */
    lua_pushcfunction(L, lua_edit);
    lua_setglobal(L, "edit");
    
    /* Register mkdir function */
    lua_pushcfunction(L, lua_mkdir);
    lua_setglobal(L, "mkdir");
    
    /* Register WiFi API */
    wifi_register_lua(L);
    
    lua_error_flag = 0;
    lua_error_msg[0] = '\0';
    
    return L;
}

/* Load Lua program */
int lua_load_program(lua_State *L, const char *code, const char *name) {
    lua_error_flag = 0;
    lua_error_msg[0] = '\0';
    
    if (luaL_loadbuffer(L, code, strlen(code), name)) {
        const char *err = lua_tostring(L, -1);
        if (err) {
            strncpy(lua_error_msg, err, sizeof(lua_error_msg) - 1);
            lua_error_msg[sizeof(lua_error_msg) - 1] = '\0';
        }
        lua_error_flag = 1;
        return 1;
    }
    
    if (lua_pcall(L, 0, 0, 0)) {
        const char *err = lua_tostring(L, -1);
        if (err) {
            strncpy(lua_error_msg, err, sizeof(lua_error_msg) - 1);
            lua_error_msg[sizeof(lua_error_msg) - 1] = '\0';
        }
        lua_error_flag = 1;
        return 1;
    }
    
    return 0;
}

/* Call setup() function */
void lua_call_setup(lua_State *L) {
    lua_getglobal(L, "setup");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0)) {
            const char *err = lua_tostring(L, -1);
            if (err) {
                strncpy(lua_error_msg, err, sizeof(lua_error_msg) - 1);
                lua_error_msg[sizeof(lua_error_msg) - 1] = '\0';
            }
            lua_error_flag = 1;
        }
    } else {
        lua_pop(L, 1);
    }
}

/* Call draw() function */
void lua_call_draw(lua_State *L) {
    lua_getglobal(L, "draw");
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0)) {
            const char *err = lua_tostring(L, -1);
            if (err) {
                strncpy(lua_error_msg, err, sizeof(lua_error_msg) - 1);
                lua_error_msg[sizeof(lua_error_msg) - 1] = '\0';
            }
            lua_error_flag = 1;
        }
    } else {
        lua_pop(L, 1);
    }
}

/* Update keyboard state in Lua */
void lua_update_keyboard(lua_State *L) {
    /* Update keyboard.state and keyboard.key */
    set_table_field_string(L, "keyboard", "state", kb_get_state());
    set_table_field_string(L, "keyboard", "key", kb_get_key());
    
    /* Update keyboard.pressed table */
    lua_getglobal(L, "keyboard");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "pressed");
        if (lua_istable(L, -1)) {
            /* Check common keys */
            const char *keys[] = {
                "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
                "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
                "escape", "return", "space", "backspace", "up", "down", "left", "right",
                NULL
            };
            
            for (int i = 0; keys[i]; i++) {
                lua_pushstring(L, keys[i]);
                if (kb_is_pressed(keys[i])) {
                    lua_pushboolean(L, 1);
                } else {
                    lua_pushnil(L);
                }
                lua_settable(L, -3);
            }
        }
        lua_pop(L, 1);  /* pop pressed table */
    }
    lua_pop(L, 1);  /* pop keyboard table */
}

/* Close Lua state */
void lua_close_load81(lua_State *L) {
    if (L) lua_close(L);
}

/* Check for errors */
int lua_had_error(lua_State *L) {
    return lua_error_flag;
}

/* Get error message */
const char *lua_get_error(lua_State *L) {
    return lua_error_msg;
}
