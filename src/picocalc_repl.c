#include "picocalc_repl.h"
#include "picocalc_keyboard.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "fat32.h"
#include "sdcard.h"
#include "lauxlib.h"
#include "lualib.h"

/* SD card Lua functions */
static int lua_fat32_is_mounted(lua_State *L) {
    lua_pushboolean(L, fat32_is_mounted());
    return 1;
}

static int lua_fat32_list_dir(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    printf("Listing directory: %s\n", path);
    
    fat32_file_t dir;
    fat32_error_t result = fat32_open(&dir, path);
    printf("fat32_open returned: %d (%s)\n", result, fat32_error_string(result));
    
    if (result != FAT32_OK) {
        lua_pushnil(L);
        lua_pushstring(L, fat32_error_string(result));
        return 2;
    }
    
    lua_newtable(L);
    int index = 1;
    fat32_entry_t entry;
    
    while (fat32_dir_read(&dir, &entry) == FAT32_OK) {
        printf("  %s (size=%lu, attr=0x%02X)\n", entry.filename, 
               (unsigned long)entry.size, entry.attr);
        lua_newtable(L);
        lua_pushstring(L, entry.filename);
        lua_setfield(L, -2, "name");
        lua_pushinteger(L, entry.size);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, entry.attr & FAT32_ATTR_DIRECTORY);
        lua_setfield(L, -2, "is_dir");
        lua_rawseti(L, -2, index++);
    }
    
    fat32_close(&dir);
    return 1;
}

static int lua_sd_reinit(lua_State *L) {
    printf("Reinitializing SD card...\n");
    /* Unmount and remount to force reinitialization */
    fat32_unmount();
    fat32_error_t result = fat32_mount();
    printf("fat32_mount: %d (%s)\n", result, fat32_error_string(result));
    lua_pushinteger(L, result);
    return 1;
}

#define REPL_LINE_MAX 256

/* Read a line from serial input */
static int read_line(char *buffer, int max_len) {
    int pos = 0;
    
    printf("> ");
    fflush(stdout);
    
    while (pos < max_len - 1) {
        /* Check for serial input */
        int c = getchar_timeout_us(100000); /* 100ms timeout */
        
        if (c == PICO_ERROR_TIMEOUT) {
            /* Poll keyboard for ESC to exit */
            kb_poll();
            if (kb_key_available()) {
                char key = kb_get_char();
                if (key == 0x1B) {
                    return -1; /* ESC pressed */
                }
            }
            continue;
        }
        
        if (c == '\r' || c == '\n') {
            printf("\n");
            buffer[pos] = '\0';
            return pos;
        }
        
        if (c == '\b' || c == 127) { /* Backspace */
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        
        if (c >= 32 && c < 127) {
            buffer[pos++] = c;
            putchar(c);
            fflush(stdout);
        }
    }
    
    buffer[pos] = '\0';
    return pos;
}

/* Run interactive Lua REPL over serial */
void repl_run(lua_State *L) {
    char line[REPL_LINE_MAX];
    
    printf("\n=== LOAD81 Lua REPL ===\n");
    printf("Type Lua commands and press ENTER\n");
    printf("Press ESC on PicoCalc keyboard to exit\n");
    printf("Try: =fat32_is_mounted(), =fat32_list_dir(\"/\"), =sd_reinit()\n\n");
    
    /* Register SD card functions for debugging */
    lua_pushcfunction(L, lua_fat32_is_mounted);
    lua_setglobal(L, "fat32_is_mounted");
    
    lua_pushcfunction(L, lua_fat32_list_dir);
    lua_setglobal(L, "fat32_list_dir");
    
    lua_pushcfunction(L, lua_sd_reinit);
    lua_setglobal(L, "sd_reinit");
    
    while (1) {
        int len = read_line(line, REPL_LINE_MAX);
        
        if (len < 0) {
            printf("\nExiting REPL...\n");
            break;
        }
        
        if (len == 0) continue;
        
        /* Handle special commands */
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        
        /* Check if line starts with '=' for expression evaluation */
        const char *code = line;
        char expr_buffer[REPL_LINE_MAX + 16];
        if (line[0] == '=') {
            snprintf(expr_buffer, sizeof(expr_buffer), "return %s", line + 1);
            code = expr_buffer;
        }
        
        /* Execute the line */
        int status = luaL_loadstring(L, code);
        if (status == 0) {
            status = lua_pcall(L, 0, LUA_MULTRET, 0);
        }
        
        if (status != 0) {
            /* Error */
            const char *msg = lua_tostring(L, -1);
            printf("Error: %s\n", msg ? msg : "unknown error");
            lua_pop(L, 1);
        } else {
            /* Print results */
            int nresults = lua_gettop(L);
            if (nresults > 0) {
                for (int i = 1; i <= nresults; i++) {
                    if (lua_isstring(L, i)) {
                        printf("%s\n", lua_tostring(L, i));
                    } else if (lua_isboolean(L, i)) {
                        printf("%s\n", lua_toboolean(L, i) ? "true" : "false");
                    } else if (lua_isnumber(L, i)) {
                        printf("%g\n", lua_tonumber(L, i));
                    } else if (lua_isnil(L, i)) {
                        printf("nil\n");
                    } else {
                        printf("%s\n", luaL_typename(L, i));
                    }
                }
                lua_pop(L, nresults);
            }
        }
    }
}
