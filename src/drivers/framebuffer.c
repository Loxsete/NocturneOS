#include <drivers/framebuffer.h>
#include <console/font.h>
#include <stdint.h>

static void    *fb_addr;
uint64_t        fb_width;
uint64_t        fb_height;
static uint64_t fb_pitch;
static uint16_t fb_bpp;
static uint32_t fb_bytes_per_pixel;

uint32_t fb_cursor_x = 0;
uint32_t fb_cursor_y = 0;
uint32_t fb_font_scale = 1;

void fb_init(void *addr, uint64_t width, uint64_t height, uint64_t pitch, uint16_t bpp)
{
    fb_addr            = addr;
    fb_width           = width;
    fb_height          = height;
    fb_pitch           = pitch;
    fb_bpp             = bpp;
    fb_bytes_per_pixel = bpp / 8;
}

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb_width || y >= fb_height) return;
    uint8_t *pixel = (uint8_t *)fb_addr + y * fb_pitch + x * fb_bytes_per_pixel;
    pixel[0] = (color >>  0) & 0xFF;
    pixel[1] = (color >>  8) & 0xFF;
    pixel[2] = (color >> 16) & 0xFF;
    if (fb_bytes_per_pixel == 4)
        pixel[3] = 0xFF;
}

void fb_fill(uint32_t color)
{
    for (uint64_t y = 0; y < fb_height; y++)
        for (uint64_t x = 0; x < fb_width; x++)
            fb_putpixel(x, y, color);
}

void fb_putchar(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg)
{
    if ((unsigned char)c >= 128) c = '?';
    uint8_t *glyph = font8x16[(unsigned char)c];
    for (int row = 0; row < FONT_HEIGHT; row++)
        for (int col = 0; col < FONT_WIDTH; col++) {
            int bit = (glyph[row] >> (7 - col)) & 1;
            uint32_t color = bit ? fg : bg;
            for (uint32_t sy = 0; sy < fb_font_scale; sy++)
                for (uint32_t sx = 0; sx < fb_font_scale; sx++)
                    fb_putpixel(x + col * fb_font_scale + sx,
                                y + row * fb_font_scale + sy, color);
        }
}

void fb_puts(uint32_t x, uint32_t y, const char *s, uint32_t fg, uint32_t bg)
{
    uint32_t cx = x;
    uint32_t cw = FONT_WIDTH  * fb_font_scale;
    uint32_t ch = FONT_HEIGHT * fb_font_scale;
    while (*s) {
        if (*s == '\n') { cx = x; y += ch; }
        else { fb_putchar(cx, y, *s, fg, bg); cx += cw; }
        s++;
    }
}

void fb_scroll(uint32_t bg)
{
    uint32_t ch = FONT_HEIGHT * fb_font_scale;
    uint8_t *dst = (uint8_t *)fb_addr;
    uint8_t *src = (uint8_t *)fb_addr + ch * fb_pitch;
    uint64_t rows_to_move = fb_height - ch;
    for (uint64_t row = 0; row < rows_to_move; row++) {
        uint8_t *d = dst + row * fb_pitch;
        uint8_t *s = src + row * fb_pitch;
        for (uint64_t col = 0; col < fb_width * fb_bytes_per_pixel; col++)
            d[col] = s[col];
    }
    for (uint64_t row = rows_to_move; row < fb_height; row++) {
        uint8_t *p = dst + row * fb_pitch;
        for (uint64_t col = 0; col < fb_width; col++) {
            p[col * fb_bytes_per_pixel + 0] = (bg >>  0) & 0xFF;
            p[col * fb_bytes_per_pixel + 1] = (bg >>  8) & 0xFF;
            p[col * fb_bytes_per_pixel + 2] = (bg >> 16) & 0xFF;
            if (fb_bytes_per_pixel == 4)
                p[col * fb_bytes_per_pixel + 3] = 0xFF;
        }
    }
}

void fb_newline(uint32_t bg)
{
    uint32_t ch = FONT_HEIGHT * fb_font_scale;
    fb_cursor_x  = 0;
    fb_cursor_y += ch;
    if (fb_cursor_y + ch > fb_height) {
        fb_scroll(bg);
        fb_cursor_y -= ch;
    }
}

void fb_putchar_cursor(char c, uint32_t fg, uint32_t bg)
{
    uint32_t cw = FONT_WIDTH  * fb_font_scale;

    if (c == '\n') { fb_newline(bg); return; }
    if (c == '\b') {
        if (fb_cursor_x >= cw) {
            fb_cursor_x -= cw;
            fb_putchar(fb_cursor_x, fb_cursor_y, ' ', fg, bg);
        }
        return;
    }
    fb_putchar(fb_cursor_x, fb_cursor_y, c, fg, bg);
    fb_cursor_x += cw;
    if (fb_cursor_x + cw > (uint32_t)fb_width)
        fb_newline(bg);
}

static void fb_putcodepoint(uint32_t cp, uint32_t fg, uint32_t bg)
{
    uint32_t cw = FONT_WIDTH  * fb_font_scale;
    uint32_t ch = FONT_HEIGHT * fb_font_scale;

    if (cp >= 0x2800 && cp <= 0x28FF) {
        uint8_t dots = cp & 0xFF;
        int dot_col[8] = {0,0,0,1,1,1,0,1};
        int dot_row[8] = {0,1,2,0,1,2,3,3};
        int dw = (int)cw / 2;
        int dh = (int)ch / 4;
        for (uint32_t y = 0; y < ch; y++)
            for (uint32_t x = 0; x < cw; x++)
                fb_putpixel(fb_cursor_x + x, fb_cursor_y + y, bg);
        for (int d = 0; d < 8; d++) {
            if (!(dots & (1 << d))) continue;
            int px = (int)fb_cursor_x + dot_col[d] * dw + dw / 4;
            int py = (int)fb_cursor_y + dot_row[d] * dh + dh / 4;
            for (int y = 0; y < dh - 1; y++)
                for (int x = 0; x < dw - 1; x++)
                    fb_putpixel((uint32_t)(px + x), (uint32_t)(py + y), fg);
        }
        fb_cursor_x += cw;
        if (fb_cursor_x + cw > (uint32_t)fb_width)
            fb_newline(bg);
        return;
    }
    char c = (cp < 128) ? (char)cp : '?';
    fb_putchar_cursor(c, fg, bg);
}

static uint32_t utf8_cp   = 0;
static int      utf8_left = 0;

void fb_putchar_cursor_utf8(char byte, uint32_t fg, uint32_t bg)
{
    uint8_t b = (uint8_t)byte;
    if (b < 0x80) {
        utf8_left = 0;
        fb_putchar_cursor(byte, fg, bg);
    } else if (b >= 0xF0) {
        utf8_cp   = b & 0x07;
        utf8_left = 3;
    } else if (b >= 0xE0) {
        utf8_cp   = b & 0x0F;
        utf8_left = 2;
    } else if (b >= 0xC0) {
        utf8_cp   = b & 0x1F;
        utf8_left = 1;
    } else {
        if (utf8_left > 0) {
            utf8_cp = (utf8_cp << 6) | (b & 0x3F);
            utf8_left--;
            if (utf8_left == 0)
                fb_putcodepoint(utf8_cp, fg, bg);
        }
    }
}
