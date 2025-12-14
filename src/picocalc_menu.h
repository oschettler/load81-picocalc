#ifndef PICOCALC_MENU_H
#define PICOCALC_MENU_H

#include <stdbool.h>

#define MAX_MENU_ITEMS 32
#define MAX_FILENAME_LEN 64

/* Menu item structure */
typedef struct {
    char filename[MAX_FILENAME_LEN];
    char display_name[MAX_FILENAME_LEN];
} MenuItem;

/* Initialize menu system */
void menu_init(void);

/* Load programs from /load81/ directory */
int menu_load_programs(void);

/* Display menu and let user select a program */
/* Returns index of selected program, or -1 if cancelled */
int menu_select_program(void);

/* Get number of menu items */
int menu_get_count(void);

/* Get menu item by index */
const MenuItem *menu_get_item(int index);

/* Load program file content into buffer */
/* Returns allocated string (caller must free), or NULL on error */
char *menu_load_file(const char *filename);

#endif /* PICOCALC_MENU_H */
