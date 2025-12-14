#ifndef PICOCALC_EDITOR_H
#define PICOCALC_EDITOR_H

#include <stdbool.h>

/* Initialize editor */
void editor_init(void);

/* Run editor for a file */
/* Returns: 0 = saved and exit, 1 = cancelled, 2 = run program */
int editor_run(const char *filename);

/* Check if editor is available */
bool editor_available(void);

#endif /* PICOCALC_EDITOR_H */
