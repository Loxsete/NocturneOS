#pragma once
#include <stdint.h>

extern uint32_t fb_font_scale; /* like 1, 2, 3... */


// Initialize the framebuffer
void fb_init(void *addr, uint64_t width, uint64_t height, uint64_t pitch, uint16_t bpp);

// Put a pixel on the framebuffer
void fb_putpixel(uint32_t x, uint32_t y, uint32_t color);

// Fill the framebuffer with a color
void fb_fill(uint32_t color);

// Draw a character to the framebuffer
void fb_putchar(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

// Draw a string to the framebuffer
void fb_puts(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg);

// Draw a character at the cursor position
void fb_putchar_cursor(char c, uint32_t fg, uint32_t bg);

// Scroll the framebuffer
void fb_scroll(uint32_t bg);

// Move to a new line
void fb_newline(uint32_t bg);

// Draw a UTF-8 character at the cursor position
void fb_putchar_cursor_utf8(char byte, uint32_t fg, uint32_t bg);

#define COLOR_BG     0x0D0D1A
#define COLOR_FG     0xFFFFFF
#define COLOR_PROMPT 0x00CC44
#define COLOR_INPUT  0xFFFFFF
#define COLOR_DIR    0x4488FF


extern uint32_t fb_cursor_x;
extern uint32_t fb_cursor_y;
extern uint64_t fb_width;
extern uint64_t fb_height;
