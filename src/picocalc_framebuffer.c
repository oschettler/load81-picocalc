#include "picocalc_framebuffer.h"
#include "lcd.h"
#include <stdlib.h>
#include <string.h>

/* Global framebuffer - 320x320x2 = 200KB */
static uint16_t fb_pixels[FB_WIDTH * FB_HEIGHT];
PicoFrameBuffer g_fb;

/* RGB565 to RGB888 conversion helpers */
static inline int rgb565_to_r(uint16_t c) { return ((c >> 11) & 0x1F) << 3; }
static inline int rgb565_to_g(uint16_t c) { return ((c >> 5) & 0x3F) << 2; }
static inline int rgb565_to_b(uint16_t c) { return (c & 0x1F) << 3; }

/* Initialize framebuffer */
void fb_init(void) {
    g_fb.pixels = fb_pixels;
    g_fb.width = FB_WIDTH;
    g_fb.height = FB_HEIGHT;
    fb_clear();
}

/* Set pixel with alpha blending. Coordinates: (0,0) = bottom-left (LOAD81 style) */
void fb_set_pixel(int x, int y, int r, int g, int b, int alpha) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    
    /* Fix display orientation: keep X as-is, flip Y coordinate */
    int fb_x = x;
    int fb_y = (FB_HEIGHT - 1) - y;
    int idx = fb_y * FB_WIDTH + fb_x;
    
    if (alpha >= 255) {
        /* Fully opaque - no blending needed */
        fb_pixels[idx] = RGB565(r, g, b);
    } else if (alpha > 0) {
        /* Alpha blending */
        uint16_t existing = fb_pixels[idx];
        int er = rgb565_to_r(existing);
        int eg = rgb565_to_g(existing);
        int eb = rgb565_to_b(existing);
        
        /* Blend: new_color = (alpha * src + (255-alpha) * dst) / 255 */
        int nr = (alpha * r + (255 - alpha) * er) / 255;
        int ng = (alpha * g + (255 - alpha) * eg) / 255;
        int nb = (alpha * b + (255 - alpha) * eb) / 255;
        
        fb_pixels[idx] = RGB565(nr, ng, nb);
    }
}

/* Get pixel color */
void fb_get_pixel(int x, int y, int *r, int *g, int *b) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) {
        *r = *g = *b = 0;
        return;
    }
    
    /* Convert from LOAD81 coordinates */
    int fb_y = (FB_HEIGHT - 1) - y;
    uint16_t pixel = fb_pixels[fb_y * FB_WIDTH + x];
    
    *r = rgb565_to_r(pixel);
    *g = rgb565_to_g(pixel);
    *b = rgb565_to_b(pixel);
}

/* Fill background with solid color */
void fb_fill_background(int r, int g, int b) {
    uint16_t color = RGB565(r, g, b);
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb_pixels[i] = color;
    }
}

/* Present framebuffer to LCD */
void fb_present(void) {
    /* Transfer entire framebuffer to LCD */
    lcd_blit(fb_pixels, 0, 0, FB_WIDTH, FB_HEIGHT);
}

/* Clear to black */
void fb_clear(void) {
    memset(fb_pixels, 0, sizeof(fb_pixels));
}
