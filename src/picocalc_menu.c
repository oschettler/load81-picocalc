#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include "debug.h"
#include "picocalc_menu.h"
#include "picocalc_framebuffer.h"
#include "picocalc_graphics.h"
#include "picocalc_keyboard.h"
#include "picocalc_wifi.h"
#include "fat32.h"
#include "build_version.h"
#include "pico/cyw43_arch.h"
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
    
    DEBUG_PRINTF("Loading programs from /load81/ directory...\n");
    
    /* Try to open the /load81 directory - FAT32 will mount automatically if needed */
    fat32_file_t dir;
    fat32_entry_t entry;
    fat32_error_t result = fat32_open(&dir, "/load81");
    DEBUG_PRINTF("fat32_open(\"/load81\") returned: %d (%s)\n", result, fat32_error_string(result));
    
    if (result == FAT32_OK) {
        DEBUG_PRINTF("Directory opened successfully, reading files...\n");
        
        /* Read directory entries - must check filename[0] for end of directory */
        do {
            result = fat32_dir_read(&dir, &entry);
            if (result != FAT32_OK) {
                DEBUG_PRINTF("Error reading directory: %s\n", fat32_error_string(result));
                break;
            }
            
            /* Check for end of directory (empty filename) */
            if (!entry.filename[0]) {
                break;
            }
            
            DEBUG_PRINTF("Found file: '%s' (attr=0x%02X, size=%lu)\n", 
                   entry.filename, entry.attr, (unsigned long)entry.size);
            
            /* Skip directories */
            if (entry.attr & FAT32_ATTR_DIRECTORY) {
                DEBUG_PRINTF("  -> Skipping (directory)\n");
                continue;
            }
            
            /* Check if it's a .lua file */
            int len = strlen(entry.filename);
            if (len > 4 && strcmp(&entry.filename[len-4], ".lua") == 0) {
                DEBUG_PRINTF("  -> Adding to menu\n");
                if (menu_count < MAX_MENU_ITEMS) {
                    strncpy(menu_items[menu_count].filename, entry.filename, MAX_FILENAME_LEN - 1);
                    strncpy(menu_items[menu_count].display_name, entry.filename, MAX_FILENAME_LEN - 1);
                    menu_count++;
                }
            } else {
                DEBUG_PRINTF("  -> Skipping (not .lua)\n");
            }
        } while (entry.filename[0] && menu_count < MAX_MENU_ITEMS);
        
        fat32_close(&dir);
    } else {
        DEBUG_PRINTF("Could not open /load81/ directory, error: %d (%s)\n", result, fat32_error_string(result));
    }
    
    /* Always add REPL as first option */
    if (menu_count < MAX_MENU_ITEMS) {
        /* Shift existing items down */
        for (int i = menu_count; i > 0; i--) {
            menu_items[i] = menu_items[i-1];
        }
        strncpy(menu_items[0].filename, "**REPL**", MAX_FILENAME_LEN - 1);
        strncpy(menu_items[0].display_name, "[REPL]", MAX_FILENAME_LEN - 1);
        menu_count++;
    }
    
    /* Add [New file] option as second item */
    if (menu_count < MAX_MENU_ITEMS) {
        /* Shift existing items down (except REPL) */
        for (int i = menu_count; i > 1; i--) {
            menu_items[i] = menu_items[i-1];
        }
        strncpy(menu_items[1].filename, "**NEWFILE**", MAX_FILENAME_LEN - 1);
        strncpy(menu_items[1].display_name, "[New file]", MAX_FILENAME_LEN - 1);
        menu_count++;
    }
    
    /* If no files found, add default */
    if (menu_count == 1) {
        DEBUG_PRINTF("No .lua files found, adding default program\n");
        strncpy(menu_items[1].filename, "default", MAX_FILENAME_LEN - 1);
        strncpy(menu_items[1].display_name, "Default Program", MAX_FILENAME_LEN - 1);
        menu_count = 2;
    }
    
    DEBUG_PRINTF("Found %d program(s) total:\n", menu_count);
    for (int i = 0; i < menu_count; i++) {
        DEBUG_PRINTF("  [%d] %s (%s)\n", i, menu_items[i].display_name, menu_items[i].filename);
    }
    
    return menu_count;
}

/* Display menu and select program */
int menu_select_program(void) {
    if (menu_count == 0) return -1;
    
    int selected = 0;
    int scroll_offset = 0;
    const int items_per_screen = 14;  /* Reduced from 15 to prevent last item cutoff */
    
    while (1) {
        /* Clear screen */
        fb_fill_background(0, 0, 50);
        
        /* Draw title aligned with IP address */
        g_draw_r = 255; g_draw_g = 255; g_draw_b = 0; g_draw_alpha = 255;
        gfx_draw_string(10, 305, "LOAD81 on PicoCalc", 18);
        
        /* Draw WiFi status/IP in top right */
        const char *wifi_status = wifi_get_status_string();
        const char *wifi_ip = wifi_get_ip_string();
        
        /* Always try to show IP if we have one, otherwise show status */
        if (strcmp(wifi_ip, "0.0.0.0") != 0) {
            /* Have IP address - show it in green */
            g_draw_r = 100; g_draw_g = 255; g_draw_b = 100; g_draw_alpha = 255;
            /* Position IP address - screen is 320px wide, font is 9px per char (8px + 1px spacing)
             * For IP like "192.168.178.122" (15 chars), need 135px width
             * Start at x=180 to fit within 320px with margin: 180 + 135 = 315 */
            gfx_draw_string(180, 305, wifi_ip, strlen(wifi_ip));
        } else {
            /* No IP - show status in blue/gray */
            g_draw_r = 150; g_draw_g = 150; g_draw_b = 255; g_draw_alpha = 255;
            gfx_draw_string(240, 305, wifi_status, strlen(wifi_status));
        }
        
        g_draw_r = 200; g_draw_g = 200; g_draw_b = 200; g_draw_alpha = 255;
        gfx_draw_string(10, 285, "Select a program:", 17);
        
        /* Draw menu items */
        int y = 255;
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
        
        /* Draw instructions at bottom */
        g_draw_r = 150; g_draw_g = 150; g_draw_b = 150; g_draw_alpha = 255;
        gfx_draw_string(10, 30, "UP/DOWN: Select  ENTER: Load", 29);
        gfx_draw_string(10, 15, "E: Edit  ESC: Cancel", 20);
        
        /* Draw build version in lower right corner */
        char build_str[32];
        snprintf(build_str, sizeof(build_str), "v%s b%d", BUILD_VERSION, BUILD_NUMBER);
        int build_len = strlen(build_str);
        /* Position in lower right: 320 - (len * 9) - 5px margin */
        int build_x = 320 - (build_len * 9) - 5;
        g_draw_r = 100; g_draw_g = 100; g_draw_b = 100; g_draw_alpha = 255;
        gfx_draw_string(build_x, 15, build_str, build_len);
        
        /* Present to screen */
        fb_present();
        
        /* Wait for key with network polling */
        kb_reset_events();
        char key = 0;
        while (!kb_key_available()) {
            cyw43_arch_poll();  /* Poll network stack for incoming connections */
            sleep_ms(10);
        }
        key = kb_get_char();
        
        /* Debug: print key code */
        DEBUG_PRINTF("Key pressed: 0x%02X ('%c')\n", (unsigned char)key, 
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

/* Generate unique filename */
static char *generate_unique_filename(void) {
    static int file_counter = 1;
    char *filename = (char *)malloc(MAX_FILENAME_LEN);
    if (!filename) return NULL;
    
    /* Try to find a unique filename */
    for (int i = file_counter; i < 1000; i++) {
        snprintf(filename, MAX_FILENAME_LEN, "program%d.lua", i);
        
        /* Check if file exists */
        char fullpath[256];
        snprintf(fullpath, sizeof(fullpath), "/load81/%s", filename);
        fat32_file_t file;
        fat32_error_t result = fat32_open(&file, fullpath);
        
        if (result == FAT32_ERROR_FILE_NOT_FOUND) {
            /* File doesn't exist - this name is available */
            file_counter = i + 1;
            return filename;
        } else if (result == FAT32_OK) {
            /* File exists - try next number */
            fat32_close(&file);
        } else {
            /* Some other error - use this name anyway */
            file_counter = i + 1;
            return filename;
        }
    }
    
    /* Fallback if we somehow can't find a unique name */
    snprintf(filename, MAX_FILENAME_LEN, "program%d.lua", file_counter++);
    return filename;
}

/* Create new empty file */
static char *create_new_file(void) {
    char *filename = generate_unique_filename();
    if (!filename) {
        DEBUG_PRINTF("Failed to generate filename\n");
        return NULL;
    }
    
    DEBUG_PRINTF("Creating new file: %s\n", filename);
    
    /* Build full path */
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "/load81/%s", filename);
    
    /* Create empty program template */
    const char *template_prog =
        "-- New LOAD81 Program\n"
        "\n"
        "function setup()\n"
        "    -- Initialize your program here\n"
        "end\n"
        "\n"
        "function draw()\n"
        "    background(0, 0, 0)\n"
        "    fill(255, 255, 255, 1)\n"
        "    text(20, 160, \"Hello, LOAD81!\")\n"
        "end\n";
    
    /* Write file */
    fat32_file_t file;
    fat32_error_t result = fat32_create(&file, fullpath);
    if (result != FAT32_OK) {
        DEBUG_PRINTF("Error creating file: %s\n", fat32_error_string(result));
        free(filename);
        return NULL;
    }
    
    size_t bytes_written = 0;
    result = fat32_write(&file, template_prog, strlen(template_prog), &bytes_written);
    fat32_close(&file);
    
    if (result != FAT32_OK) {
        DEBUG_PRINTF("Error writing file: %s\n", fat32_error_string(result));
        free(filename);
        return NULL;
    }
    
    DEBUG_PRINTF("Created new file: %s (%zu bytes)\n", filename, bytes_written);
    return filename;
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
    
    /* If it's the new file marker, create a new file */
    if (strcmp(filename, "**NEWFILE**") == 0) {
        char *new_filename = create_new_file();
        if (new_filename) {
            /* Load the newly created file */
            char *content = menu_load_file(new_filename);
            free(new_filename);
            return content;
        }
        return strdup(default_prog);
    }
    
    /* If it's the default program, return the default code */
    if (strcmp(filename, "default") == 0) {
        return strdup(default_prog);
    }
    
    /* Build full path */
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "/load81/%s", filename);
    DEBUG_PRINTF("Loading file: %s\n", fullpath);
    
    /* Open the file */
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, fullpath);
    if (result != FAT32_OK) {
        DEBUG_PRINTF("Error opening file: %s\n", fat32_error_string(result));
        return strdup(default_prog);
    }
    
    /* Get file size */
    uint32_t file_size = fat32_size(&file);
    DEBUG_PRINTF("File size: %lu bytes\n", (unsigned long)file_size);
    
    if (file_size == 0 || file_size > 65536) {
        DEBUG_PRINTF("Invalid file size\n");
        fat32_close(&file);
        return strdup(default_prog);
    }
    
    /* Allocate buffer for file content */
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        DEBUG_PRINTF("Failed to allocate memory\n");
        fat32_close(&file);
        return strdup(default_prog);
    }
    
    /* Read file content */
    size_t bytes_read = 0;
    result = fat32_read(&file, buffer, file_size, &bytes_read);
    if (result != FAT32_OK) {
        DEBUG_PRINTF("Error reading file: %s\n", fat32_error_string(result));
        free(buffer);
        fat32_close(&file);
        return strdup(default_prog);
    }
    
    DEBUG_PRINTF("Read %zu bytes from file\n", bytes_read);
    
    /* Null-terminate the buffer */
    buffer[bytes_read] = '\0';
    
    /* Close the file */
    fat32_close(&file);
    
    return buffer;
}
