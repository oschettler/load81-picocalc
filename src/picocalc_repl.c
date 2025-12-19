#include "picocalc_repl.h"
#include "picocalc_keyboard.h"
#include "picocalc_framebuffer.h"
#include "picocalc_graphics.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "debug.h"
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
    
    fat32_file_t dir;
    fat32_error_t result = fat32_open(&dir, path);
    
    if (result != FAT32_OK) {
        lua_pushnil(L);
        lua_pushstring(L, fat32_error_string(result));
        return 2;
    }
    
    lua_newtable(L);
    int index = 1;
    fat32_entry_t entry;
    
    while (fat32_dir_read(&dir, &entry) == FAT32_OK) {
        if (!entry.filename[0]) break;  /* End of directory */
        
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
    /* Unmount and remount to force reinitialization */
    fat32_unmount();
    fat32_error_t result = fat32_mount();
    lua_pushinteger(L, result);
    return 1;
}

#define REPL_LINE_MAX 256
#define REPL_HISTORY_SIZE 100
#define SCREEN_LINES 18  /* Number of lines visible on screen */

/* REPL state */
static char repl_history[REPL_HISTORY_SIZE][REPL_LINE_MAX];
static int history_count = 0;
static int history_scroll = 0;  /* 0 = showing most recent */
static char input_buffer[REPL_LINE_MAX];
static int input_pos = 0;

/* Add line to history */
static void add_to_history(const char *line) {
    if (history_count < REPL_HISTORY_SIZE) {
        strncpy(repl_history[history_count], line, REPL_LINE_MAX - 1);
        repl_history[history_count][REPL_LINE_MAX - 1] = '\0';
        history_count++;
    } else {
        /* Shift history up */
        for (int i = 0; i < REPL_HISTORY_SIZE - 1; i++) {
            strcpy(repl_history[i], repl_history[i + 1]);
        }
        strncpy(repl_history[REPL_HISTORY_SIZE - 1], line, REPL_LINE_MAX - 1);
        repl_history[REPL_HISTORY_SIZE - 1][REPL_LINE_MAX - 1] = '\0';
    }
    history_scroll = 0;  /* Reset scroll to show newest */
}

/* Draw the REPL screen */
static void draw_repl_screen(void) {
    fb_fill_background(0, 0, 20);
    
    /* Draw title */
    g_draw_r = 255; g_draw_g = 255; g_draw_b = 0; g_draw_alpha = 255;
    gfx_draw_string(10, 305, "LOAD81 Lua REPL", 15);
    
    /* Draw help text */
    g_draw_r = 150; g_draw_g = 150; g_draw_b = 150; g_draw_alpha = 255;
    gfx_draw_string(10, 15, "ESC: Exit  UP/DN: Scroll", 24);
    
    /* Draw history */
    int y = 285;
    int start_line = history_count - SCREEN_LINES - history_scroll;
    if (start_line < 0) start_line = 0;
    int end_line = history_count - history_scroll;
    
    for (int i = start_line; i < end_line && y > 35; i++) {
        if (i >= 0 && i < history_count) {
            /* Determine color based on line type */
            if (repl_history[i][0] == '>') {
                /* Input line */
                g_draw_r = 100; g_draw_g = 255; g_draw_b = 100; g_draw_alpha = 255;
            } else if (strncmp(repl_history[i], "Error:", 6) == 0) {
                /* Error line */
                g_draw_r = 255; g_draw_g = 100; g_draw_b = 100; g_draw_alpha = 255;
            } else {
                /* Output line */
                g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
            }
            
            /* Truncate long lines */
            char display_line[41];
            strncpy(display_line, repl_history[i], 40);
            display_line[40] = '\0';
            
            gfx_draw_string(10, y, display_line, strlen(display_line));
            y -= 14;
        }
    }
    
    /* Draw input prompt */
    g_draw_r = 100; g_draw_g = 255; g_draw_b = 100; g_draw_alpha = 255;
    gfx_draw_string(10, 30, "> ", 2);
    
    /* Draw input buffer */
    g_draw_r = 255; g_draw_g = 255; g_draw_b = 255; g_draw_alpha = 255;
    char display_input[38];
    int display_start = 0;
    if (input_pos > 37) {
        display_start = input_pos - 37;
    }
    strncpy(display_input, input_buffer + display_start, 37);
    display_input[37] = '\0';
    gfx_draw_string(26, 30, display_input, strlen(display_input));
    
    /* Draw cursor */
    int cursor_x = 26 + (input_pos - display_start) * 8;
    if (cursor_x < 310) {
        g_draw_r = 255; g_draw_g = 255; g_draw_b = 0; g_draw_alpha = 128;
        gfx_draw_box(cursor_x, 28, cursor_x + 7, 40);
    }
    
    fb_present();
}

/* Execute Lua code and add result to history */
static void execute_lua(lua_State *L, const char *code) {
    /* Add input to history */
    char input_line[REPL_LINE_MAX];
    snprintf(input_line, sizeof(input_line), "> %s", code);
    add_to_history(input_line);
    
    /* Check if line starts with '=' for expression evaluation */
    char expr_buffer[REPL_LINE_MAX + 16];
    if (code[0] == '=') {
        snprintf(expr_buffer, sizeof(expr_buffer), "return %s", code + 1);
        code = expr_buffer;
    }
    
    /* Execute the code */
    int status = luaL_loadstring(L, code);
    if (status == 0) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
    }
    
    if (status != 0) {
        /* Error */
        const char *msg = lua_tostring(L, -1);
        if (msg) {
            /* Split error message into multiple lines if needed */
            char error_line[REPL_LINE_MAX];
            snprintf(error_line, sizeof(error_line), "Error: %s", msg);
            
            /* Add error to history, splitting if too long */
            int len = strlen(error_line);
            int pos = 0;
            while (pos < len) {
                char chunk[41];
                strncpy(chunk, error_line + pos, 40);
                chunk[40] = '\0';
                add_to_history(chunk);
                pos += 40;
            }
        } else {
            add_to_history("Error: unknown error");
        }
        lua_pop(L, 1);
    } else {
        /* Print results */
        int nresults = lua_gettop(L);
        if (nresults > 0) {
            for (int i = 1; i <= nresults; i++) {
                char result_line[REPL_LINE_MAX];
                if (lua_isstring(L, i)) {
                    snprintf(result_line, sizeof(result_line), "%s", lua_tostring(L, i));
                } else if (lua_isboolean(L, i)) {
                    snprintf(result_line, sizeof(result_line), "%s", 
                            lua_toboolean(L, i) ? "true" : "false");
                } else if (lua_isnumber(L, i)) {
                    snprintf(result_line, sizeof(result_line), "%g", lua_tonumber(L, i));
                } else if (lua_isnil(L, i)) {
                    snprintf(result_line, sizeof(result_line), "nil");
                } else {
                    snprintf(result_line, sizeof(result_line), "%s", luaL_typename(L, i));
                }
                add_to_history(result_line);
            }
            lua_pop(L, nresults);
        }
    }
}

/* Run interactive Lua REPL on screen */
void repl_run(lua_State *L) {
    /* Initialize REPL state */
    history_count = 0;
    history_scroll = 0;
    input_pos = 0;
    input_buffer[0] = '\0';
    
    /* Add welcome message */
    add_to_history("=== LOAD81 Lua REPL ===");
    add_to_history("Type Lua code and press ENTER");
    add_to_history("Prefix with = to evaluate");
    add_to_history("Try: =2+2, =math.pi");
    add_to_history("");
    
    /* Register SD card functions */
    lua_pushcfunction(L, lua_fat32_is_mounted);
    lua_setglobal(L, "fat32_is_mounted");
    
    lua_pushcfunction(L, lua_fat32_list_dir);
    lua_setglobal(L, "fat32_list_dir");
    
    lua_pushcfunction(L, lua_sd_reinit);
    lua_setglobal(L, "sd_reinit");
    
    bool running = true;
    while (running) {
        draw_repl_screen();
        
        /* Wait for key with network polling */
        kb_reset_events();
        char key = 0;
        while (!kb_key_available()) {
            cyw43_arch_poll();  /* Poll network stack for file server */
            sleep_ms(10);
        }
        key = kb_get_char();
        
        /* Handle input */
        if (key == 0xB1) {  /* ESC */
            running = false;
        } else if (key == 0x0D || key == 0x0A) {  /* ENTER */
            if (input_pos > 0) {
                execute_lua(L, input_buffer);
                input_buffer[0] = '\0';
                input_pos = 0;
            }
        } else if (key == 0x08 || key == 0x7F) {  /* BACKSPACE */
            if (input_pos > 0) {
                input_pos--;
                input_buffer[input_pos] = '\0';
            }
        } else if (key == 0xB5) {  /* UP - scroll history up */
            if (history_scroll < history_count - SCREEN_LINES) {
                history_scroll++;
            }
        } else if (key == 0xB6) {  /* DOWN - scroll history down */
            if (history_scroll > 0) {
                history_scroll--;
            }
        } else if (key >= 32 && key < 127 && input_pos < REPL_LINE_MAX - 1) {
            /* Printable character */
            input_buffer[input_pos++] = key;
            input_buffer[input_pos] = '\0';
        }
    }
}
