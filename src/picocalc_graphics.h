#ifndef PICOCALC_GRAPHICS_H
#define PICOCALC_GRAPHICS_H

#include "picocalc_framebuffer.h"

/* Current drawing color and alpha */
extern int g_draw_r, g_draw_g, g_draw_b, g_draw_alpha;

/* Drawing primitives - compatible with LOAD81 API */

/* Draw a filled rectangle */
void gfx_draw_box(int x1, int y1, int x2, int y2);

/* Draw a filled ellipse */
void gfx_draw_ellipse(int xc, int yc, int rx, int ry);

/* Draw a filled triangle */
void gfx_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3);

/* Draw a line */
void gfx_draw_line(int x1, int y1, int x2, int y2);

/* Draw a horizontal line (optimized) */
void gfx_draw_hline(int x1, int x2, int y);

/* Bitmap font rendering */
void gfx_draw_char(int x, int y, int c);
void gfx_draw_string(int x, int y, const char *s, int len);

#endif /* PICOCALC_GRAPHICS_H */
