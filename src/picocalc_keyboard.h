#ifndef PICOCALC_KEYBOARD_H
#define PICOCALC_KEYBOARD_H

#include <stdbool.h>

/* Key names for LOAD81 keyboard.pressed[] table */
#define MAX_KEY_NAME_LEN 32

/* Initialize keyboard system */
void kb_init(void);

/* Poll keyboard for events */
void kb_poll(void);

/* Check if a key is currently pressed (by name) */
bool kb_is_pressed(const char *keyname);

/* Get keyboard event state for Lua */
const char *kb_get_state(void);     /* "down", "up", or "none" */
const char *kb_get_key(void);       /* Last key name */

/* Reset event state (call each frame) */
void kb_reset_events(void);

/* Wait for a key press (blocking) */
char kb_wait_key(void);

/* Check if any key is available */
bool kb_key_available(void);

/* Get raw key character */
char kb_get_char(void);

#endif /* PICOCALC_KEYBOARD_H */
