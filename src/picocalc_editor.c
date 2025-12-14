#include "picocalc_editor.h"
#include "picocalc_framebuffer.h"
#include "picocalc_graphics.h"
#include "picocalc_keyboard.h"
#include "keyboard.h"
#include "fat32.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Font dimensions for 8x10 font */
#define FONT_WIDTH 8
#define FONT_HEIGHT 10
#define FONT_KERNING 8  /* Horizontal spacing between characters */

/* Display margins */
#define MARGIN_TOP 10
#define MARGIN_BOTTOM 10
#define MARGIN_LEFT 30  /* Space for line numbers */
#define MARGIN_RIGHT 10

/* Syntax highlight types */
#define HL_NORMAL 0
#define HL_ERROR 1
#define HL_COMMENT 2
#define HL_KEYWORD 3
#define HL_STRING 4
#define HL_NUMBER 5
#define HL_FUNCDEF 6
#define HL_LIB 7

/* Key repeat timing */
#define KEY_REPEAT_PERIOD 2
#define KEY_REPEAT_PERIOD_FAST 1
#define KEY_REPEAT_DELAY 8

#define KEY_MAX 32

/* Editor row - represents one line of text */
typedef struct erow {
    int size;              /* Size of the row, excluding null term */
    char *chars;           /* Row content */
    unsigned char *hl;     /* Syntax highlight type for each character */
} erow;

/* Key state for tracking pressed keys */
typedef struct keyState {
    char key;              /* Key character or special key code */
    int counter;           /* Counter for key repeat */
} keyState;

/* Syntax highlight color scheme */
typedef struct hlcolor {
    int r, g, b;
} hlcolor;

static hlcolor hlscheme[] = {
    {200, 200, 200},  /* HL_NORMAL */
    {255, 0, 0},      /* HL_ERROR */
    {180, 180, 0},    /* HL_COMMENT */
    {50, 255, 50},    /* HL_KEYWORD */
    {0, 255, 255},    /* HL_STRING */
    {225, 100, 100},  /* HL_NUMBER */
    {255, 255, 255},  /* HL_FUNCDEF */
    {255, 0, 255}     /* HL_LIB */
};

/* Global editor state */
static struct editorConfig {
    int cx, cy;           /* Cursor x and y position in characters */
    unsigned char cblink; /* Cursor blink counter */
    int screenrows;       /* Number of rows that we can show */
    int screencols;       /* Number of cols that we can show */
    int rowoff;           /* Row offset on screen (scrolling) */
    int coloff;           /* Column offset on screen (scrolling) */
    int numrows;          /* Number of rows */
    erow *row;            /* Rows */
    keyState key[KEY_MAX];/* Remember if a key is pressed */
    int dirty;            /* File modified but not saved */
    char *filename;       /* Currently open filename */
    char *err;            /* Error string to display */
    int errline;          /* Error line to highlight */
    uint32_t last_key_time; /* Last key press time for repeat */
} E;

/* ====================== Syntax highlight ==================== */

static int is_separator(int c) {
    return c == '\0' || isspace(c) || strchr(",.()+-/*=~%[];",c) != NULL;
}

/* Update syntax highlighting for a row */
static void editorUpdateSyntax(erow *row) {
    int i, prev_sep, in_string;
    char *p;
    char *keywords[] = {
        /* Keywords */
        "function","if","while","for","end","in","do","local","break",
        "then","pairs","return","else","elseif","not","and","or",
        /* Libs (ending with dots) will be marked as HL_LIB */
        "math.","table.","string.","mouse.","keyboard.",NULL
    };

    row->hl = realloc(row->hl, row->size);
    memset(row->hl, HL_NORMAL, row->size);

    /* Point to the first non-space char */
    p = row->chars;
    i = 0;
    while(*p && isspace(*p)) {
        p++;
        i++;
    }
    prev_sep = 1;
    in_string = 0;
    
    while(*p) {
        if (prev_sep && *p == '-' && *(p+1) == '-') {
            /* Comment from here to end */
            memset(row->hl+i, HL_COMMENT, row->size-i);
            return;
        }
        
        /* Handle strings "" and '' */
        if (in_string) {
            row->hl[i] = HL_STRING;
            if (*p == '\\') {
                if (i+1 < row->size) row->hl[i+1] = HL_STRING;
                p += 2; i += 2;
                prev_sep = 0;
                continue;
            }
            if (*p == in_string) in_string = 0;
            p++; i++;
            continue;
        } else {
            if (*p == '"' || *p == '\'') {
                in_string = *p;
                row->hl[i] = HL_STRING;
                p++; i++;
                prev_sep = 0;
                continue;
            }
        }
        
        /* Handle numbers */
        if ((isdigit(*p) && (prev_sep || row->hl[i-1] == HL_NUMBER)) ||
            (*p == '.' && i > 0 && row->hl[i-1] == HL_NUMBER)) {
            row->hl[i] = HL_NUMBER;
            p++; i++;
            prev_sep = 0;
            continue;
        }

        /* Handle keywords and lib calls */
        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int lib = keywords[j][klen-1] == '.';

                if (!lib && !memcmp(p, keywords[j], klen) &&
                    is_separator(*(p+klen)))
                {
                    /* Keyword */
                    memset(row->hl+i, HL_KEYWORD, klen);
                    p += klen;
                    i += klen;
                    break;
                }
                if (lib && !memcmp(p, keywords[j], klen)) {
                    /* Library call */
                    memset(row->hl+i, HL_LIB, klen);
                    p += klen;
                    i += klen;
                    while(!is_separator(*p)) {
                        row->hl[i] = HL_LIB;
                        p++;
                        i++;
                    }
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(*p);
        p++; i++;
    }
}

/* ======================= Editor rows implementation ======================= */

/* Insert a row at the specified position */
static void editorInsertRow(int at, char *s) {
    if (at > E.numrows) return;
    E.row = realloc(E.row, sizeof(erow)*(E.numrows+1));
    if (at != E.numrows)
        memmove(E.row+at+1, E.row+at, sizeof(E.row[0])*(E.numrows-at));
    E.row[at].size = strlen(s);
    E.row[at].chars = strdup(s);
    E.row[at].hl = NULL;
    editorUpdateSyntax(E.row+at);
    E.numrows++;
    E.dirty++;
}

/* Free row's heap allocated stuff */
static void editorFreeRow(erow *row) {
    free(row->chars);
    free(row->hl);
}

/* Remove the row at the specified position */
static void editorDelRow(int at) {
    erow *row;

    if (at >= E.numrows) return;
    row = E.row+at;
    editorFreeRow(row);
    memmove(E.row+at, E.row+at+1, sizeof(E.row[0])*(E.numrows-at-1));
    E.numrows--;
    E.dirty++;
}

/* Turn the editor rows into a single heap-allocated string */
static char *editorRowsToString(int *buflen) {
    char *buf = NULL, *p;
    int totlen = 0;
    int j;

    /* Compute count of bytes */
    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size+1; /* +1 for "\n" */
    *buflen = totlen;
    totlen++; /* Also make space for nulterm */

    p = buf = malloc(totlen);
    for (j = 0; j < E.numrows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

/* Insert a character at the specified position in a row */
static void editorRowInsertChar(erow *row, int at, int c) {
    if (at > row->size) {
        /* Pad the string with spaces */
        int padlen = at-row->size;
        row->chars = realloc(row->chars, row->size+padlen+2);
        memset(row->chars+row->size, ' ', padlen);
        row->chars[row->size+padlen+1] = '\0';
        row->size += padlen+1;
    } else {
        row->chars = realloc(row->chars, row->size+2);
        memmove(row->chars+at+1, row->chars+at, row->size-at+1);
        row->size++;
    }
    row->chars[at] = c;
    editorUpdateSyntax(row);
    E.dirty++;
}

/* Append the string 's' at the end of a row */
static void editorRowAppendString(erow *row, char *s) {
    int l = strlen(s);

    row->chars = realloc(row->chars, row->size+l+1);
    memcpy(row->chars+row->size, s, l);
    row->size += l;
    row->chars[row->size] = '\0';
    editorUpdateSyntax(row);
    E.dirty++;
}

/* Delete character at position in row */
static void editorRowDelChar(erow *row, int at) {
    if (row->size <= at) return;
    memmove(row->chars+at, row->chars+at+1, row->size-at);
    editorUpdateSyntax(row);
    row->size--;
    E.dirty++;
}

/* Insert character at cursor position */
static void editorInsertChar(int c) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    /* Add empty rows if needed */
    if (!row) {
        while(E.numrows <= filerow)
            editorInsertRow(E.numrows, "");
    }
    row = &E.row[filerow];
    editorRowInsertChar(row, filecol, c);
    if (E.cx == E.screencols-1)
        E.coloff++;
    else
        E.cx++;
    E.dirty++;
}

/* Insert a newline */
static void editorInsertNewline(void) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    if (!row) {
        if (filerow == E.numrows) {
            editorInsertRow(filerow, "");
            goto fixcursor;
        }
        return;
    }
    if (filecol >= row->size) filecol = row->size;
    if (filecol == 0) {
        editorInsertRow(filerow, "");
    } else {
        /* Split line */
        editorInsertRow(filerow+1, row->chars+filecol);
        row = &E.row[filerow];
        row->chars[filecol] = '\0';
        row->size = filecol;
        editorUpdateSyntax(row);
    }
fixcursor:
    if (E.cy == E.screenrows-1) {
        E.rowoff++;
    } else {
        E.cy++;
    }
    E.cx = 0;
    E.coloff = 0;
}

/* Delete character at cursor */
static void editorDelChar(void) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    if (!row || (filecol == 0 && filerow == 0)) return;
    if (filecol == 0) {
        /* Move current line to end of previous line */
        filecol = E.row[filerow-1].size;
        editorRowAppendString(&E.row[filerow-1], row->chars);
        editorDelRow(filerow);
        row = NULL;
        if (E.cy == 0)
            E.rowoff--;
        else
            E.cy--;
        E.cx = filecol;
        if (E.cx >= E.screencols) {
            int shift = (E.screencols-E.cx)+1;
            E.cx -= shift;
            E.coloff += shift;
        }
    } else {
        editorRowDelChar(row, filecol-1);
        if (E.cx == 0 && E.coloff)
            E.coloff--;
        else
            E.cx--;
    }
    if (row) editorUpdateSyntax(row);
    E.dirty++;
}

/* Program template for new files */
static char *editorTemplate[] = {
    "function setup()",
    "   -- This function is called only once at startup.",
    "end",
    "",
    "function draw()",
    "   -- This function is called at every frame refresh.",
    "    background(0,0,0)",
    "    fill(200,200,200,255)",
    "    text(WIDTH/2-100,HEIGHT/2,\"Hello PicoCalc!\")",
    "end",
    NULL
};

/* Load file into editor */
static int editorOpen(char *filename) {
    fat32_file_t file;
    fat32_error_t result;

    E.dirty = 0;
    free(E.filename);
    E.filename = strdup(filename);
    
    printf("[Editor] Attempting to open file: '%s'\n", filename);
    result = fat32_open(&file, filename);
    if (result != FAT32_OK) {
        /* No such file, add a template */
        printf("[Editor] File not found (error: %s), using template\n", fat32_error_string(result));
        int j = 0;
        while(editorTemplate[j])
            editorInsertRow(E.numrows, editorTemplate[j++]);
        return 1;
    }
    
    printf("[Editor] File opened successfully\n");
    
    /* Get file size */
    uint32_t file_size = fat32_size(&file);
    printf("[Editor] File size: %lu bytes\n", (unsigned long)file_size);
    
    if (file_size > 65536) {
        printf("[Editor] File too large\n");
        fat32_close(&file);
        int j = 0;
        while(editorTemplate[j])
            editorInsertRow(E.numrows, editorTemplate[j++]);
        return 1;
    }
    
    /* Allocate buffer for entire file */
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        printf("[Editor] Failed to allocate memory\n");
        fat32_close(&file);
        int j = 0;
        while(editorTemplate[j])
            editorInsertRow(E.numrows, editorTemplate[j++]);
        return 1;
    }
    
    /* Read entire file */
    size_t bytes_read = 0;
    result = fat32_read(&file, buffer, file_size, &bytes_read);
    if (result != FAT32_OK) {
        printf("[Editor] Error reading file: %s\n", fat32_error_string(result));
        free(buffer);
        fat32_close(&file);
        int j = 0;
        while(editorTemplate[j])
            editorInsertRow(E.numrows, editorTemplate[j++]);
        return 1;
    }
    
    buffer[bytes_read] = '\0';
    printf("[Editor] Read %zu bytes\n", bytes_read);
    
    /* Parse buffer line by line */
    char *line_start = buffer;
    int line_count = 0;
    for (size_t i = 0; i <= bytes_read; i++) {
        if (i == bytes_read || buffer[i] == '\n' || buffer[i] == '\r') {
            /* End of line */
            buffer[i] = '\0';
            editorInsertRow(E.numrows, line_start);
            line_count++;
            
            /* Skip \r\n or \n\r sequences */
            if (i < bytes_read && (buffer[i+1] == '\n' || buffer[i+1] == '\r') && buffer[i+1] != buffer[i]) {
                i++;
            }
            
            line_start = &buffer[i+1];
        }
    }
    
    printf("[Editor] Parsed %d lines\n", line_count);
    
    free(buffer);
    fat32_close(&file);
    E.dirty = 0;
    return 0;
}

/* Save file */
static int editorSave(char *filename) {
    int len;
    char *buf = editorRowsToString(&len);
    fat32_file_t file;
    fat32_error_t result;

    result = fat32_open(&file, filename);
    if (result != FAT32_OK) {
        /* Try to create the file */
        printf("[Editor] Creating new file: %s\n", filename);
    }
    
    /* Write the buffer */
    size_t bytes_written = 0;
    result = fat32_write(&file, buf, len, &bytes_written);
    if (result != FAT32_OK) {
        printf("[Editor] Error writing file: %s\n", fat32_error_string(result));
        free(buf);
        fat32_close(&file);
        return 1;
    }
    
    printf("[Editor] Wrote %zu bytes to file\n", bytes_written);
    fat32_close(&file);
    free(buf);
    E.dirty = 0;
    return 0;
}

/* ============================= Editor drawing ============================= */

/* Draw cursor */
static void editorDrawCursor(void) {
    int x = E.cx*FONT_KERNING + MARGIN_LEFT;
    /* Calculate Y from top - PicoCalc Y=0 is at bottom, so invert */
    int y = FB_HEIGHT - MARGIN_TOP - (E.cy+1)*FONT_HEIGHT;

    /* Blink cursor */
    if (!(E.cblink & 0x80)) {
        /* Draw cursor as inverse box */
        g_draw_r = 100; g_draw_g = 100; g_draw_b = 255; g_draw_alpha = 128;
        gfx_draw_box(x, y, x+FONT_KERNING-1, y+FONT_HEIGHT-1);
    }
    E.cblink += 4;
}

/* Draw all characters */
static void editorDrawChars(void) {
    int y, x;
    erow *r;
    char buf[16];

    for (y = 0; y < E.screenrows; y++) {
        int chary, filerow = E.rowoff+y;

        if (filerow >= E.numrows) break;
        /* Calculate Y from top - PicoCalc Y=0 is at bottom, so invert */
        chary = FB_HEIGHT - MARGIN_TOP - (y+1)*FONT_HEIGHT;
        r = &E.row[filerow];

        /* Draw line number */
        snprintf(buf, sizeof(buf), "%3d", filerow+1);
        g_draw_r = 120; g_draw_g = 120; g_draw_b = 120; g_draw_alpha = 255;
        gfx_draw_string(2, chary, buf, strlen(buf));

        /* Draw line content with syntax highlighting */
        for (x = 0; x < E.screencols; x++) {
            int idx = x+E.coloff;
            int charx;
            hlcolor *color;

            if (idx >= r->size) break;
            charx = x*FONT_KERNING + MARGIN_LEFT;
            color = hlscheme + r->hl[idx];
            g_draw_r = color->r;
            g_draw_g = color->g;
            g_draw_b = color->b;
            g_draw_alpha = 255;
            gfx_draw_char(charx, chary, r->chars[idx]);
        }
    }
    
    /* Draw error message if any */
    if (E.err) {
        g_draw_r = 255; g_draw_g = 0; g_draw_b = 0; g_draw_alpha = 255;
        gfx_draw_string(MARGIN_LEFT, 20, E.err, strlen(E.err));
    }
    
    /* Draw status bar at bottom */
    char status[64];
    snprintf(status, sizeof(status), "%s%s", E.filename ? E.filename : "unnamed",
             E.dirty ? " [+]" : "");
    g_draw_r = 255; g_draw_g = 255; g_draw_b = 255; g_draw_alpha = 255;
    gfx_draw_string(2, 5, status, strlen(status));
}

/* Main draw function */
static void editorDraw(void) {
    /* Clear screen to dark blue */
    fb_fill_background(0, 0, 50);
    
    /* Draw editor contents */
    editorDrawChars();
    editorDrawCursor();
    
    /* Present to screen */
    fb_present();
}

/* ========================= Editor events handling  ======================== */

/* Check if key should auto-repeat */
static int pressed_or_repeated(int counter) {
    int period;

    if (counter > KEY_REPEAT_DELAY+(KEY_REPEAT_PERIOD*3)) {
        period = KEY_REPEAT_PERIOD_FAST;
    } else {
        period = KEY_REPEAT_PERIOD;
    }
    if (counter > 1 && counter < KEY_REPEAT_DELAY) return 0;
    return ((counter+period-1) % period) == 0;
}

/* Move cursor */
static void editorMoveCursor(char key) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    int rowlen;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    switch(key) {
    case 'L': /* Left */
        if (E.cx == 0) {
            if (E.coloff) E.coloff--;
        } else {
            E.cx -= 1;
        }
        break;
    case 'R': /* Right */
        if (row && filecol < row->size) {
            if (E.cx == E.screencols-1) {
                E.coloff++;
            } else {
                E.cx += 1;
            }
        }
        break;
    case 'U': /* Up */
        if (E.cy == 0) {
            if (E.rowoff) E.rowoff--;
        } else {
            E.cy -= 1;
        }
        break;
    case 'D': /* Down */
        if (filerow < E.numrows) {
            if (E.cy == E.screenrows-1) {
                E.rowoff++;
            } else {
                E.cy += 1;
            }
        }
        break;
    }
    
    /* Fix cx if the current line has not enough chars */
    filerow = E.rowoff+E.cy;
    filecol = E.coloff+E.cx;
    row = (filerow >= E.numrows) ? NULL : &E.row[filerow];
    rowlen = row ? row->size : 0;
    if (filecol > rowlen) {
        E.cx -= filecol-rowlen;
        if (E.cx < 0) {
            E.coloff += E.cx;
            E.cx = 0;
        }
    }
}

/* Get key state entry */
static keyState *editorGetKeyState(char key) {
    int free = -1;
    for (int j = 0; j < KEY_MAX; j++) {
        if (E.key[j].key == key) return &E.key[j];
        if (E.key[j].key == 0) free = j;
    }
    if (free == -1) return NULL;
    E.key[free].key = key;
    E.key[free].counter = 0;
    return &E.key[free];
}

/* Main event loop - returns 1 to exit, 0 to continue */
static int editorEvents(void) {
    /* Poll keyboard */
    kb_poll();
    
    /* Check for character input */
    if (kb_key_available()) {
        char ch = kb_get_char();
        
        if (ch == KEY_ESC) { /* ESC key */
            return 1; /* Exit editor */
        } else if (ch == KEY_RETURN || ch == KEY_ENTER) {
            editorInsertNewline();
            E.cblink = 0;
        } else if (ch == KEY_BACKSPACE || ch == KEY_DEL) {
            editorDelChar();
            E.cblink = 0;
        } else if (ch == KEY_LEFT) {
            editorMoveCursor('L');
            E.cblink = 0;
        } else if (ch == KEY_RIGHT) {
            editorMoveCursor('R');
            E.cblink = 0;
        } else if (ch == KEY_UP) {
            editorMoveCursor('U');
            E.cblink = 0;
        } else if (ch == KEY_DOWN) {
            editorMoveCursor('D');
            E.cblink = 0;
        } else if (ch >= 32 && ch < 127) { /* Printable character */
            editorInsertChar(ch);
            E.cblink = 0;
        }
        
        E.last_key_time = to_ms_since_boot(get_absolute_time());
    }
    
    /* Draw editor */
    editorDraw();
    
    /* Small delay */
    sleep_ms(33); /* ~30 FPS */
    
    return 0;
}

/* ========================= Public API ======================== */

void editor_init(void) {
    memset(&E, 0, sizeof(E));
    E.screencols = (FB_WIDTH - MARGIN_LEFT - MARGIN_RIGHT) / FONT_KERNING;
    E.screenrows = (FB_HEIGHT - MARGIN_TOP - MARGIN_BOTTOM) / FONT_HEIGHT;
}

bool editor_available(void) {
    return true; /* Editor is now available */
}

int editor_run(const char *filename) {
    /* Initialize editor state */
    E.cx = 0;
    E.cy = 0;
    E.cblink = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.err = NULL;
    E.errline = 0;
    memset(E.key, 0, sizeof(E.key));
    
    /* Load the file */
    editorOpen((char*)filename);
    
    /* Main editor loop */
    while (!editorEvents()) {
        /* Loop until ESC is pressed */
    }
    
    /* Save if modified */
    int result = 0;
    if (E.dirty) {
        /* Auto-save on exit */
        if (editorSave(E.filename) != 0) {
            result = 1; /* Error saving */
        }
    }
    
    /* Clean up */
    for (int i = 0; i < E.numrows; i++) {
        editorFreeRow(&E.row[i]);
    }
    free(E.row);
    free(E.filename);
    free(E.err);
    
    return result;
}
