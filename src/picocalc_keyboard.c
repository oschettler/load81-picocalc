#include "picocalc_keyboard.h"
#include "keyboard.h"
#include <string.h>
#include <ctype.h>

/* Keyboard state */
static char current_state[16] = "none";  /* "down", "up", "none" */
static char current_key[MAX_KEY_NAME_LEN] = "";
static char pressed_keys[256] = {0};  /* Track which keys are currently pressed */

/* Map key codes to key names */
static const char *get_key_name(char key) {
    static char name_buf[MAX_KEY_NAME_LEN];
    
    /* Special keys */
    if (key == 0x1B) return "escape";
    if (key == 0x08) return "backspace";
    if (key == 0x09) return "tab";
    if (key == 0x0D || key == 0x0A) return "return";
    if (key == 0x20) return "space";
    if (key == 0x7F) return "delete";
    
    /* Arrow keys (ANSI sequences) */
    if (key == 0x1B) {  /* ESC sequence for arrows */
        return "escape";  /* Will be handled by sequence parser */
    }
    
    /* Alphanumeric - convert to lowercase */
    if (key >= 'A' && key <= 'Z') {
        name_buf[0] = tolower(key);
        name_buf[1] = '\0';
        return name_buf;
    }
    
    if (key >= 'a' && key <= 'z') {
        name_buf[0] = key;
        name_buf[1] = '\0';
        return name_buf;
    }
    
    if (key >= '0' && key <= '9') {
        name_buf[0] = key;
        name_buf[1] = '\0';
        return name_buf;
    }
    
    /* Default: convert to string */
    name_buf[0] = key;
    name_buf[1] = '\0';
    return name_buf;
}

/* Initialize keyboard */
void kb_init(void) {
    keyboard_init();
    keyboard_set_background_poll(true);
    memset(pressed_keys, 0, sizeof(pressed_keys));
    strcpy(current_state, "none");
    current_key[0] = '\0';
}

/* Poll keyboard for events */
void kb_poll(void) {
    keyboard_poll();
}

/* Reset event state (called each frame) */
void kb_reset_events(void) {
    strcpy(current_state, "none");
    current_key[0] = '\0';
    /* Clear all pressed keys each frame. Since PicoCalc doesn't send KEYUP events,
     * keys will be re-set if they're still being pressed when kb_get_char() is called. */
    memset(pressed_keys, 0, sizeof(pressed_keys));
}

/* Check if a key is currently pressed */
bool kb_is_pressed(const char *keyname) {
    /* Simple lookup in pressed_keys array */
    if (!keyname || !keyname[0]) return false;
    
    /* Single character keys */
    if (strlen(keyname) == 1) {
        char c = keyname[0];
        return pressed_keys[(unsigned char)c] != 0;
    }
    
    /* Special keys - check by name */
    if (strcmp(keyname, "escape") == 0) return pressed_keys[0x1B] != 0;
    if (strcmp(keyname, "return") == 0) return pressed_keys[0x0D] != 0;
    if (strcmp(keyname, "space") == 0) return pressed_keys[0x20] != 0;
    if (strcmp(keyname, "backspace") == 0) return pressed_keys[0x08] != 0;
    if (strcmp(keyname, "tab") == 0) return pressed_keys[0x09] != 0;
    if (strcmp(keyname, "delete") == 0) return pressed_keys[0x7F] != 0;
    
    /* Arrow keys - map to special values */
    if (strcmp(keyname, "up") == 0) return pressed_keys[0x80] != 0;
    if (strcmp(keyname, "down") == 0) return pressed_keys[0x81] != 0;
    if (strcmp(keyname, "left") == 0) return pressed_keys[0x82] != 0;
    if (strcmp(keyname, "right") == 0) return pressed_keys[0x83] != 0;
    
    return false;
}

/* Get keyboard state */
const char *kb_get_state(void) {
    return current_state;
}

/* Get last key */
const char *kb_get_key(void) {
    return current_key;
}

/* Check if key is available */
bool kb_key_available(void) {
    return keyboard_key_available();
}

/* Get raw key character */
char kb_get_char(void) {
    if (!keyboard_key_available()) return 0;
    
    char key = keyboard_get_key();
    
    /* Update state */
    const char *keyname = get_key_name(key);
    strcpy(current_state, "down");
    strncpy(current_key, keyname, MAX_KEY_NAME_LEN - 1);
    current_key[MAX_KEY_NAME_LEN - 1] = '\0';
    
    /* Mark key as pressed (don't clear other keys - let them persist) */
    pressed_keys[(unsigned char)key] = 1;
    
    /* Map PicoCalc arrow keys to special indices for kb_is_pressed() */
    if (key == 0xB5) {  /* UP arrow */
        pressed_keys[0x80] = 1;
    } else if (key == 0xB6) {  /* DOWN arrow */
        pressed_keys[0x81] = 1;
    } else if (key == 0xB4) {  /* LEFT arrow */
        pressed_keys[0x82] = 1;
    } else if (key == 0xB7) {  /* RIGHT arrow */
        pressed_keys[0x83] = 1;
    }
    
    return key;
}

/* Wait for key press (blocking) */
char kb_wait_key(void) {
    while (!keyboard_key_available()) {
        keyboard_poll();
        sleep_ms(10);
    }
    return kb_get_char();
}
