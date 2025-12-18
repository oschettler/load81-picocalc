/*
 * LOAD81 for PicoCalc
 * Main application entry point
 */

#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

/* Debug output support */
#include "debug.h"

/* PicoCalc drivers */
#include "lcd.h"
#include "keyboard.h"
#include "southbridge.h"
#include "fat32.h"

/* LOAD81 modules */
#include "picocalc_framebuffer.h"
#include "picocalc_graphics.h"
#include "picocalc_keyboard.h"
#include "picocalc_lua.h"
#include "picocalc_menu.h"
#include "picocalc_editor.h"
#include "picocalc_wifi.h"
#include "picocalc_nex.h"
#include "picocalc_repl.h"

#ifdef ENABLE_9P_SERVER
/* 9P Server Core 1 functions */
extern void p9_core1_launch(void);
extern bool p9_server_is_active(void);
#endif

#define FPS 30
#define FRAME_TIME_MS (1000 / FPS)

/* Global state */
static lua_State *g_lua = NULL;
static bool g_program_running = false;
static uint64_t g_frame_count = 0;

/* Keyboard interrupt callback (required by PicoCalc keyboard driver) */
void user_interrupt(void) {
    /* Stub - keyboard events handled by polling */
}

/* Initialize hardware */
static bool init_hardware(void) {
    /* Initialize Pico stdlib (only for debug output) */
    DEBUG_INIT();
    
    /* Initialize southbridge (power, keyboard interface) */
    sb_init();
    
    /* Initialize LCD */
    lcd_init();
    lcd_clear_screen();
    
    /* Disable text cursor (LOAD81 is a graphics application) */
    lcd_enable_cursor(false);
    
    /* Initialize keyboard */
    kb_init();
    
    /* Initialize framebuffer */
    fb_init();
    
    /* Initialize SD card subsystem - mounting happens lazily when files are accessed */
    DEBUG_PRINTF("Initializing SD card subsystem...\n");
    fat32_init();
    
    /* Initialize WiFi (non-blocking) */
    wifi_init();
    
    /* Initialize NEX */
    nex_init();
    
#ifdef ENABLE_9P_SERVER
    /* Launch Core 1 with 9P server */
    DEBUG_PRINTF("Launching 9P server on Core 1...\n");
    p9_core1_launch();
    DEBUG_PRINTF("9P server core launched\n");
#endif
    
    return true;
}

/* Show splash screen */
static void show_splash(void) {
    DEBUG_PRINTF("LOAD81: Starting splash screen\n");
    DEBUG_PRINTF("FB_WIDTH=%d, FB_HEIGHT=%d\n", FB_WIDTH, FB_HEIGHT);
    
    fb_fill_background(0, 0, 50);
    DEBUG_PRINTF("Background filled\n");
    
    g_draw_r = 255; g_draw_g = 255; g_draw_b = 0; g_draw_alpha = 255;
    DEBUG_PRINTF("Drawing text at (60, 180)\n");
#ifdef DEBUG_OUTPUT
    gfx_draw_string(60, 180, "LOAD81 for PicoCalc (debug)", 19);
#else
    gfx_draw_string(60, 180, "LOAD81 for PicoCalc", 19);
#endif    
    g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
    gfx_draw_string(80, 150, "Version 1.0", 11);
    
    g_draw_r = 150; g_draw_g = 150; g_draw_b = 150; g_draw_alpha = 255;
    gfx_draw_string(40, 120, "A Lua Fantasy Console", 21);
    gfx_draw_string(40, 100, "for Clockwork PicoCalc", 22);
    
    DEBUG_PRINTF("Presenting framebuffer\n");
    fb_present();
    sleep_ms(2000);
}

/* Format error message: remove decimal from line numbers and wrap at 35 chars */
static void format_error_message(const char *err, char *out, size_t out_size) {
    char temp[512];
    const char *src = err;
    char *dst = temp;
    size_t remaining = sizeof(temp) - 1;
    
    /* First pass: remove decimal points from line numbers */
    while (*src && remaining > 0) {
        if (*src == ':' && *(src + 1) >= '0' && *(src + 1) <= '9') {
            /* Found line number after colon */
            *dst++ = *src++;  /* Copy colon */
            remaining--;
            
            /* Copy digits, skip decimal point */
            while (*src && remaining > 0) {
                if (*src >= '0' && *src <= '9') {
                    *dst++ = *src++;
                    remaining--;
                } else if (*src == '.') {
                    src++;  /* Skip decimal point */
                    /* Skip remaining decimal digits */
                    while (*src >= '0' && *src <= '9') {
                        src++;
                    }
                    break;
                } else {
                    break;
                }
            }
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }
    *dst = '\0';
    
    /* Second pass: wrap at 35 characters */
    src = temp;
    dst = out;
    remaining = out_size - 1;
    int line_len = 0;
    
    while (*src && remaining > 0) {
        if (line_len >= 35 && *src == ' ') {
            /* Insert newline at space */
            *dst++ = '\n';
            remaining--;
            line_len = 0;
            src++;  /* Skip the space */
        } else if (line_len >= 35) {
            /* Force wrap */
            *dst++ = '\n';
            remaining--;
            line_len = 0;
        } else {
            *dst++ = *src++;
            remaining--;
            line_len++;
            if (*(src - 1) == '\n') {
                line_len = 0;
            }
        }
    }
    *dst = '\0';
}

/* Draw multi-line error message */
static void draw_error_lines(int x, int y, const char *text) {
    const char *line_start = text;
    int line_y = y;
    
    while (*line_start) {
        const char *line_end = strchr(line_start, '\n');
        int line_len;
        
        if (line_end) {
            line_len = line_end - line_start;
        } else {
            line_len = strlen(line_start);
        }
        
        if (line_len > 0) {
            gfx_draw_string(x, line_y, line_start, line_len);
            line_y -= 12;  /* Move down for next line */
        }
        
        if (line_end) {
            line_start = line_end + 1;
        } else {
            break;
        }
    }
}

/* Main program loop */
static void program_loop(lua_State *L) {
    g_program_running = true;
    g_frame_count = 0;
    
    /* Call setup() once */
    lua_call_setup(L);
    
    if (lua_had_error(L)) {
        /* Show error */
        fb_fill_background(50, 0, 0);
        g_draw_r = 255; g_draw_g = 255; g_draw_b = 255; g_draw_alpha = 255;
        gfx_draw_string(10, 220, "Lua Error in setup():", 21);
        g_draw_r = 255; g_draw_g = 100; g_draw_b = 100; g_draw_alpha = 255;
        
        char formatted_err[512];
        format_error_message(lua_get_error(L), formatted_err, sizeof(formatted_err));
        draw_error_lines(10, 200, formatted_err);
        
        g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
        gfx_draw_string(10, 20, "Press any key", 13);
        fb_present();
        kb_wait_key();
        g_program_running = false;
        return;
    }
    
    /* Main loop */
    while (g_program_running) {
        uint32_t frame_start = to_ms_since_boot(get_absolute_time());
        
        /* Poll keyboard */
        kb_poll();
        
        /* Check for ESC to exit */
        if (kb_key_available()) {
            char key = kb_get_char();
            if (key == 0xB1) {  /* ESC (PicoCalc key code) */
                g_program_running = false;
                break;
            }
        }
        
        /* Update keyboard state in Lua */
        lua_update_keyboard(L);
        
        /* Call draw() */
        lua_call_draw(L);
        
        if (lua_had_error(L)) {
            /* Show error */
            fb_fill_background(50, 0, 0);
            g_draw_r = 255; g_draw_g = 255; g_draw_b = 255; g_draw_alpha = 255;
            gfx_draw_string(10, 220, "Lua Error in draw():", 20);
            g_draw_r = 255; g_draw_g = 100; g_draw_b = 100; g_draw_alpha = 255;
            
            char formatted_err[512];
            format_error_message(lua_get_error(L), formatted_err, sizeof(formatted_err));
            draw_error_lines(10, 200, formatted_err);
            
            g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
            gfx_draw_string(10, 20, "Press any key", 13);
            fb_present();
            kb_wait_key();
            g_program_running = false;
            break;
        }
        
        /* Present framebuffer to screen */
        fb_present();
        
        /* Reset keyboard events for next frame */
        kb_reset_events();
        
        g_frame_count++;
        
        /* Frame rate limiting */
        uint32_t frame_time = to_ms_since_boot(get_absolute_time()) - frame_start;
        if (frame_time < FRAME_TIME_MS) {
            sleep_ms(FRAME_TIME_MS - frame_time);
        }
    }
}

/* Main application */
int main(void) {
    /* Initialize hardware */
    DEBUG_PRINTF("\n\n=== LOAD81 for PicoCalc Starting ===\n");
    if (!init_hardware()) {
        DEBUG_PRINTF("Hardware initialization failed!\n");
        /* Blink LED to indicate error */
        while (1) {
            sleep_ms(500);
        }
    }
    DEBUG_PRINTF("Hardware initialized successfully\n");
    
    /* Show splash screen */
    show_splash();
    
    /* Execute startup script if it exists */
    DEBUG_PRINTF("[Startup] Checking for /load81/start.lua...\n");
    fat32_file_t startup_file;
    fat32_error_t result = fat32_open(&startup_file, "/load81/start.lua");
    if (result == FAT32_OK) {
        DEBUG_PRINTF("[Startup] Found start.lua, executing...\n");
        
        /* Get file size */
        uint32_t file_size = fat32_size(&startup_file);
        if (file_size > 0 && file_size < 65536) {
            /* Read startup script */
            char *startup_code = (char *)malloc(file_size + 1);
            if (startup_code) {
                size_t bytes_read = 0;
                result = fat32_read(&startup_file, startup_code, file_size, &bytes_read);
                if (result == FAT32_OK) {
                    startup_code[bytes_read] = '\0';
                    
                    /* Create Lua state for startup */
                    lua_State *startup_lua = lua_init_load81();
                    if (startup_lua) {
                        /* Register WiFi and NEX APIs */
                        wifi_register_lua(startup_lua);
                        nex_register_lua(startup_lua);
                        
                        /* Load and execute startup script */
                        if (luaL_loadstring(startup_lua, startup_code) == 0) {
                            if (lua_pcall(startup_lua, 0, 0, 0) != 0) {
                                DEBUG_PRINTF("[Startup] Error: %s\n", lua_tostring(startup_lua, -1));
                            } else {
                                DEBUG_PRINTF("[Startup] Executed successfully\n");
                            }
                        } else {
                            DEBUG_PRINTF("[Startup] Load error: %s\n", lua_tostring(startup_lua, -1));
                        }
                        
                        lua_close_load81(startup_lua);
                    }
                }
                free(startup_code);
            }
        }
        fat32_close(&startup_file);
    } else {
        DEBUG_PRINTF("[Startup] No start.lua found (this is normal)\n");
    }
    
    /* Main menu loop */
    while (1) {
        /* Initialize menu */
        menu_init();
        
        /* Load programs from SD card */
        int program_count = menu_load_programs();
        
        if (program_count == 0) {
            /* No programs found - show error */
            fb_fill_background(50, 0, 0);
            g_draw_r = 255; g_draw_g = 100; g_draw_b = 100; g_draw_alpha = 255;
            gfx_draw_string(10, 160, "No programs found!", 18);
            g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
            gfx_draw_string(10, 140, "Place .lua files in /load81/", 28);
            gfx_draw_string(10, 120, "on the SD card", 14);
            fb_present();
            sleep_ms(3000);
            continue;
        }
        
        /* Show menu and select program */
        int selected = menu_select_program();
        
        if (selected < 0) {
            /* User cancelled - show splash again and retry */
            continue;
        }
        
        /* Check if edit mode was requested (high bit set) */
        bool edit_mode = (selected & 0x8000) != 0;
        selected = selected & 0x7FFF;  /* Clear high bit */
        
        const MenuItem *item = menu_get_item(selected);
        if (!item) continue;
        
        /* Handle edit mode */
        if (edit_mode) {
            /* Cannot edit REPL or default program */
            if (strcmp(item->filename, "**REPL**") == 0 || 
                strcmp(item->filename, "default") == 0) {
                fb_fill_background(50, 20, 0);
                g_draw_r = 255; g_draw_g = 200; g_draw_b = 100; g_draw_alpha = 255;
                gfx_draw_string(10, 160, "Cannot edit this item", 21);
                fb_present();
                sleep_ms(1500);
                continue;
            }
            
            /* Build full path for editor */
            char fullpath[256];
            snprintf(fullpath, sizeof(fullpath), "/load81/%s", item->filename);
            
            /* Initialize editor */
            editor_init();
            
            /* Run editor */
            int edit_result = editor_run(fullpath);
            
            if (edit_result == 0) {
                /* File was saved successfully */
                fb_fill_background(0, 50, 0);
                g_draw_r = 100; g_draw_g = 255; g_draw_b = 100; g_draw_alpha = 255;
                gfx_draw_string(10, 160, "File saved!", 11);
                fb_present();
                sleep_ms(1000);
            }
            
            continue;  /* Return to menu */
        }
        
        /* Check if REPL was selected */
        if (strcmp(item->filename, "**REPL**") == 0) {
            /* Run REPL */
            fb_fill_background(0, 0, 0);
            fb_present();
            
            g_lua = lua_init_load81();
            if (g_lua) {
                repl_run(g_lua);
                lua_close_load81(g_lua);
                g_lua = NULL;
            }
            continue;
        }
        
        /* Load program file */
        char *program_code = menu_load_file(item->filename);
        if (!program_code) {
            /* Error loading file */
            fb_fill_background(50, 0, 0);
            g_draw_r = 255; g_draw_g = 100; g_draw_b = 100; g_draw_alpha = 255;
            gfx_draw_string(10, 160, "Error loading program!", 22);
            fb_present();
            sleep_ms(2000);
            continue;
        }
        
        /* Initialize Lua */
        g_lua = lua_init_load81();
        if (!g_lua) {
            free(program_code);
            continue;
        }
        
        /* Register WiFi and NEX APIs */
        wifi_register_lua(g_lua);
        nex_register_lua(g_lua);
        
        /* Load program */
        if (lua_load_program(g_lua, program_code, item->filename) != 0) {
            /* Error loading program */
            fb_fill_background(50, 0, 0);
            g_draw_r = 255; g_draw_g = 255; g_draw_b = 255; g_draw_alpha = 255;
            gfx_draw_string(10, 220, "Lua Error:", 10);
            g_draw_r = 255; g_draw_g = 100; g_draw_b = 100; g_draw_alpha = 255;
            
            char formatted_err[512];
            format_error_message(lua_get_error(g_lua), formatted_err, sizeof(formatted_err));
            draw_error_lines(10, 200, formatted_err);
            
            g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
            gfx_draw_string(10, 20, "Press any key", 13);
            fb_present();
            kb_wait_key();
            
            lua_close_load81(g_lua);
            g_lua = NULL;
            free(program_code);
            continue;
        }
        
        free(program_code);
        
        /* Run program */
        program_loop(g_lua);
        
        /* Clean up */
        lua_close_load81(g_lua);
        g_lua = NULL;
        
        /* Return to menu */
    }
    
    return 0;
}
