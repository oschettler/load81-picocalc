#include "picocalc_graphics.h"
#include "drivers/font.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Current drawing color */
int g_draw_r = 255;
int g_draw_g = 255;
int g_draw_b = 255;
int g_draw_alpha = 255;

/* Swap integers */
static void swap_int(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/* Draw horizontal line (optimized) */
void gfx_draw_hline(int x1, int x2, int y) {
    if (x1 > x2) swap_int(&x1, &x2);
    for (int x = x1; x <= x2; x++) {
        fb_set_pixel(x, y, g_draw_r, g_draw_g, g_draw_b, g_draw_alpha);
    }
}

/* Draw line using Bresenham's algorithm */
void gfx_draw_line(int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        fb_set_pixel(x1, y1, g_draw_r, g_draw_g, g_draw_b, g_draw_alpha);
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/* Draw filled rectangle */
void gfx_draw_box(int x1, int y1, int x2, int y2) {
    if (x1 > x2) swap_int(&x1, &x2);
    if (y1 > y2) swap_int(&y1, &y2);
    
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            fb_set_pixel(x, y, g_draw_r, g_draw_g, g_draw_b, g_draw_alpha);
        }
    }
}

/* Draw filled ellipse using midpoint algorithm */
void gfx_draw_ellipse(int xc, int yc, int rx, int ry) {
    if (rx <= 0 || ry <= 0) return;
    
    /* Draw filled ellipse by drawing horizontal lines */
    for (int y = -ry; y <= ry; y++) {
        int x = (int)(rx * sqrt(1.0 - (double)(y * y) / (double)(ry * ry)));
        gfx_draw_hline(xc - x, xc + x, yc + y);
    }
}

/* Draw filled triangle */
void gfx_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3) {
    /* Sort vertices by y-coordinate */
    if (y1 > y2) { swap_int(&y1, &y2); swap_int(&x1, &x2); }
    if (y1 > y3) { swap_int(&y1, &y3); swap_int(&x1, &x3); }
    if (y2 > y3) { swap_int(&y2, &y3); swap_int(&x2, &x3); }
    
    /* Scan convert triangle */
    for (int y = y1; y <= y3; y++) {
        int xa, xb;
        
        if (y < y2) {
            /* Top half */
            if (y2 - y1 != 0)
                xa = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            else
                xa = x1;
        } else {
            /* Bottom half */
            if (y3 - y2 != 0)
                xa = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
            else
                xa = x2;
        }
        
        if (y3 - y1 != 0)
            xb = x1 + (x3 - x1) * (y - y1) / (y3 - y1);
        else
            xb = x1;
        
        if (xa > xb) swap_int(&xa, &xb);
        gfx_draw_hline(xa, xb, y);
    }
}

/* Draw single character using PicoCalc's 8x10 font */
void gfx_draw_char(int x, int y, int c) {
    if (c < 0 || c > 255) return;
    
    const font_t *font = &font_8x10;
    int glyph_width = font->width;
    int glyph_height = GLYPH_HEIGHT;
    
    /* Get glyph data for this character */
    const uint8_t *glyph = &font->glyphs[c * glyph_height];
    
    /* Draw the glyph - render rows in reverse to fix upside-down text */
    for (int row = 0; row < glyph_height; row++) {
        uint8_t row_data = glyph[glyph_height - 1 - row];
        for (int col = 0; col < glyph_width; col++) {
            if (row_data & (0x80 >> col)) {
                fb_set_pixel(x + col, y + row, 
                           g_draw_r, g_draw_g, g_draw_b, g_draw_alpha);
            }
        }
    }
}

/* Draw string */
void gfx_draw_string(int x, int y, const char *s, int len) {
    const int char_width = 8; /* Width of 8x10 font */
    const int char_spacing = 1; /* Extra pixel between chars */
    
    for (int i = 0; i < len && s[i]; i++) {
        gfx_draw_char(x + i * (char_width + char_spacing), y, (unsigned char)s[i]);
    }
}
