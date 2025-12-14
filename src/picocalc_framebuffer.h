#ifndef PICOCALC_FRAMEBUFFER_H
#define PICOCALC_FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* PicoCalc display is 320x320 pixels, RGB565 format */
#define FB_WIDTH 320
#define FB_HEIGHT 320

/* Framebuffer structure */
typedef struct {
    uint16_t *pixels;     /* 320*320 RGB565 pixel array */
    int width;            /* 320 */
    int height;           /* 320 */
} PicoFrameBuffer;

/* Global framebuffer instance */
extern PicoFrameBuffer g_fb;

/* RGB to RGB565 conversion */
#define RGB565(r, g, b) ((uint16_t)(((r) >> 3) << 11 | ((g) >> 2) << 5 | ((b) >> 3)))

/* Initialize framebuffer */
void fb_init(void);

/* Set a pixel at x,y with RGB color and alpha blending */
void fb_set_pixel(int x, int y, int r, int g, int b, int alpha);

/* Get pixel color at x,y (returns r,g,b in provided pointers) */
void fb_get_pixel(int x, int y, int *r, int *g, int *b);

/* Fill entire framebuffer with a solid color */
void fb_fill_background(int r, int g, int b);

/* Present framebuffer to LCD display */
void fb_present(void);

/* Clear framebuffer to black */
void fb_clear(void);

#endif /* PICOCALC_FRAMEBUFFER_H */
