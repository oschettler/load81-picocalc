#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include "picocalc_menu.h"
#include "picocalc_framebuffer.h"
#include "picocalc_graphics.h"
#include "picocalc_keyboard.h"
#include "picocalc_wifi.h"
#include "fat32.h"
#include <lua.h>
#include <lauxlib.h>

static MenuItem menu_items[MAX_MENU_ITEMS];
static int menu_count = 0;

/* Initialize menu */
void menu_init(void) {
    menu_count = 0;
    memset(menu_items, 0, sizeof(menu_items));
}

/* Load programs from /load81/ directory */
int menu_load_programs(void) {
    menu_count = 0;
    
    printf("Loading programs from /load81/ directory...\n");
    
    /* Try to open the /load81 directory - FAT32 will mount automatically if needed */
    fat32_file_t dir;
    fat32_entry_t entry;
    fat32_error_t result = fat32_open(&dir, "/load81");
    printf("fat32_open(\"/load81\") returned: %d (%s)\n", result, fat32_error_string(result));
    
    if (result == FAT32_OK) {
        printf("Directory opened successfully, reading files...\n");
        
        /* Read directory entries - must check filename[0] for end of directory */
        do {
            result = fat32_dir_read(&dir, &entry);
            if (result != FAT32_OK) {
                printf("Error reading directory: %s\n", fat32_error_string(result));
                break;
            }
            
            /* Check for end of directory (empty filename) */
            if (!entry.filename[0]) {
                break;
            }
            
            printf("Found file: '%s' (attr=0x%02X, size=%lu)\n", 
                   entry.filename, entry.attr, (unsigned long)entry.size);
            
            /* Skip directories */
            if (entry.attr & FAT32_ATTR_DIRECTORY) {
                printf("  -> Skipping (directory)\n");
                continue;
            }
            
            /* Check if it's a .lua file */
            int len = strlen(entry.filename);
            if (len > 4 && strcmp(&entry.filename[len-4], ".lua") == 0) {
                printf("  -> Adding to menu\n");
                if (menu_count < MAX_MENU_ITEMS) {
                    strncpy(menu_items[menu_count].filename, entry.filename, MAX_FILENAME_LEN - 1);
                    strncpy(menu_items[menu_count].display_name, entry.filename, MAX_FILENAME_LEN - 1);
                    menu_count++;
                }
            } else {
                printf("  -> Skipping (not .lua)\n");
            }
        } while (entry.filename[0] && menu_count < MAX_MENU_ITEMS);
        
        fat32_close(&dir);
    } else {
        printf("Could not open /load81/ directory, error: %d (%s)\n", result, fat32_error_string(result));
    }
    
    /* Always add REPL as first option */
    if (menu_count < MAX_MENU_ITEMS) {
        /* Shift existing items down */
        for (int i = menu_count; i > 0; i--) {
            menu_items[i] = menu_items[i-1];
        }
        strncpy(menu_items[0].filename, "**REPL**", MAX_FILENAME_LEN - 1);
        strncpy(menu_items[0].display_name, "[Debug REPL]", MAX_FILENAME_LEN - 1);
        menu_count++;
    }
    
    /* If no files found, add default */
    if (menu_count == 1) {
        printf("No .lua files found, adding default program\n");
        strncpy(menu_items[1].filename, "default", MAX_FILENAME_LEN - 1);
        strncpy(menu_items[1].display_name, "Default Program", MAX_FILENAME_LEN - 1);
        menu_count = 2;
    }
    
    printf("Found %d program(s) total:\n", menu_count);
    for (int i = 0; i < menu_count; i++) {
        printf("  [%d] %s (%s)\n", i, menu_items[i].display_name, menu_items[i].filename);
    }
    
    return menu_count;
}

/* Display menu and select program */
int menu_select_program(void) {
    if (menu_count == 0) return -1;
    
    int selected = 0;
    int scroll_offset = 0;
    const int items_per_screen = 15;
    
    while (1) {
        /* Clear screen */
        fb_fill_background(0, 0, 50);
        
        /* Draw title */
        g_draw_r = 255; g_draw_g = 255; g_draw_b = 0; g_draw_alpha = 255;
        gfx_draw_string(10, 300, "LOAD81 for PicoCalc", 19);
        
        /* Draw WiFi status in top right */
        const char *wifi_status = wifi_get_status_string();
        const char *wifi_ip = wifi_get_ip_string();
        
        /* If online, show IP address; otherwise show status */
        if (strcmp(wifi_status, "Online") == 0) {
            /* Connected - show IP address in green */
            g_draw_r = 100; g_draw_g = 255; g_draw_b = 100; g_draw_alpha = 255;
            gfx_draw_string(220, 305, wifi_ip, strlen(wifi_ip));
        } else {
            /* Not connected - show status in blue/gray */
            g_draw_r = 150; g_draw_g = 150; g_draw_b = 255; g_draw_alpha = 255;
            gfx_draw_string(240, 305, wifi_status, strlen(wifi_status));
        }
        
        g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
        gfx_draw_string(10, 280, "Select a program:", 17);
        
        /* Draw menu items */
        int y = 250;
        for (int i = scroll_offset; i < menu_count && i < scroll_offset + items_per_screen; i++) {
            if (i == selected) {
                /* Highlight selected item */
                g_draw_r = 255; g_draw_g = 255; g_draw_b = 0; g_draw_alpha = 128;
                gfx_draw_box(5, y - 2, 315, y + 12);
                g_draw_r = 0; g_draw_g = 0; g_draw_b = 0; g_draw_alpha = 255;
            } else {
                g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
            }
            
            gfx_draw_string(10, y, menu_items[i].display_name, 
                          strlen(menu_items[i].display_name));
            y -= 16;
        }
        
        /* Draw instructions */
        g_draw_r = 150; g_draw_g = 150; g_draw_b = 150; g_draw_alpha = 255;
        gfx_draw_string(10, 30, "UP/DOWN: Select  ENTER: Load", 29);
        gfx_draw_string(10, 15, "E: Edit  ESC: Cancel", 20);
        
        /* Present to screen */
        fb_present();
        
        /* Wait for key */
        kb_reset_events();
        char key = kb_wait_key();
        
        /* Debug: print key code */
        printf("Key pressed: 0x%02X ('%c')\n", (unsigned char)key, 
               (key >= 32 && key < 127) ? key : '?');
        
        /* Handle input */
        if (key == 0xB1) {  /* ESC (PicoCalc key code) */
            return -1;
        } else if (key == 0x0D || key == 0x0A) {  /* ENTER */
            return selected;
        } else if (key == 'e' || key == 'E') {  /* Edit */
            return selected | 0x8000;  /* Set high bit to indicate edit mode */
        } else if (key == 0xB5 || key == 'w' || key == 'W') {  /* UP arrow or W */
            if (selected > 0) {
                selected--;
                if (selected < scroll_offset) scroll_offset = selected;
            }
        } else if (key == 0xB6 || key == 's' || key == 'S') {  /* DOWN arrow or S */
            if (selected < menu_count - 1) {
                selected++;
                if (selected >= scroll_offset + items_per_screen) {
                    scroll_offset = selected - items_per_screen + 1;
                }
            }
        }
    }
}

/* Get menu count */
int menu_get_count(void) {
    return menu_count;
}

/* Get menu item */
const MenuItem *menu_get_item(int index) {
    if (index < 0 || index >= menu_count) return NULL;
    return &menu_items[index];
}

/* Load file content */
char *menu_load_file(const char *filename) {
    /* Default program fallback */
    const char *default_prog = 
        "function setup()\n"
        "end\n"
        "\n"
        "function draw()\n"
        "    background(0, 0, 0)\n"
        "    fill(255, 255, 0, 1)\n"
        "    text(20, 160, \"LOAD81 for PicoCalc\")\n"
        "    fill(200, 200, 200, 1)\n"
        "    text(20, 140, \"Place .lua files in /load81/\")\n"
        "    text(20, 120, \"Press ESC to return to menu\")\n"
        "end\n";
    
    /* If it's the default program, return the default code */
    if (strcmp(filename, "default") == 0) {
        return strdup(default_prog);
    }
    
    /* Build full path */
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "/load81/%s", filename);
    printf("Loading file: %s\n", fullpath);
    
    /* Open the file */
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, fullpath);
    if (result != FAT32_OK) {
        printf("Error opening file: %s\n", fat32_error_string(result));
        return strdup(default_prog);
    }
    
    /* Get file size */
    uint32_t file_size = fat32_size(&file);
    printf("File size: %lu bytes\n", (unsigned long)file_size);
    
    if (file_size == 0 || file_size > 65536) {
        printf("Invalid file size\n");
        fat32_close(&file);
        return strdup(default_prog);
    }
    
    /* Allocate buffer for file content */
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        printf("Failed to allocate memory\n");
        fat32_close(&file);
        return strdup(default_prog);
    }
    
    /* Read file content */
    size_t bytes_read = 0;
    result = fat32_read(&file, buffer, file_size, &bytes_read);
    if (result != FAT32_OK) {
        printf("Error reading file: %s\n", fat32_error_string(result));
        free(buffer);
        fat32_close(&file);
        return strdup(default_prog);
    }
    
    printf("Read %zu bytes from file\n", bytes_read);
    
    /* Null-terminate the buffer */
    buffer[bytes_read] = '\0';
    
    /* Close the file */
    fat32_close(&file);
    
    return buffer;
}
