#include <drivers/framebuffer.h>
#include <console/font.h>
#include <stdint.h>
#include <lib/katoi.h>


static void    *fb_addr;
uint64_t        fb_width;
uint64_t        fb_height;
static uint64_t fb_pitch;
static uint16_t fb_bpp;
static uint32_t fb_bytes_per_pixel;

uint32_t fb_cursor_x = 0;
uint32_t fb_cursor_y = 0;
uint32_t fb_font_scale = 1;

static uint32_t utf8_cp   = 0;
static int      utf8_left = 0;

static uint32_t term_fg = 0xFFFFFF;
static uint32_t term_bg = 0x0D0D1A;

static int esc_state = 0;
static char esc_buf[16];
static int esc_len = 0;

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
    (void)fg;
    (void)bg;

    uint32_t cw = FONT_WIDTH * fb_font_scale;

    if (c == '\n') { fb_newline(term_bg); return; }

    if (c == '\b') {
        if (fb_cursor_x >= cw) {
            fb_cursor_x -= cw;
            fb_putchar(fb_cursor_x, fb_cursor_y, ' ', term_fg, term_bg);
        }
        return;
    }

    fb_putchar(fb_cursor_x, fb_cursor_y, c, term_fg, term_bg);
    fb_cursor_x += cw;

    if (fb_cursor_x + cw > (uint32_t)fb_width)
        fb_newline(term_bg);
}

static uint32_t ansi_256_to_rgb(int n)
{
    static const uint32_t basic[16] = {
        0x000000, 0x800000, 0x008000, 0x808000,
        0x000080, 0x800080, 0x008080, 0xC0C0C0,
        0x808080, 0xFF0000, 0x00FF00, 0xFFFF00,
        0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
    };

    if (n < 16)
        return basic[n];

    if (n >= 16 && n <= 231) {
        n -= 16;

        int r = (n / 36) % 6;
        int g = (n / 6) % 6;
        int b = n % 6;

        r = r ? r * 40 + 55 : 0;
        g = g ? g * 40 + 55 : 0;
        b = b ? b * 40 + 55 : 0;

        return (r << 16) | (g << 8) | b;
    }

    if (n >= 232 && n <= 255) {
        int v = (n - 232) * 10 + 8;
        return (v << 16) | (v << 8) | v;
    }

    return 0xFFFFFF;
}

static void handle_ansi(void)
{
    esc_buf[esc_len] = 0;

    int nums[8];
    int count = 0;

    char *p = esc_buf;

    while (*p && count < 8) {
        nums[count++] = katoi(p);

        while (*p && *p != ';')
            p++;

        if (*p == ';')
            p++;
    }

    for (int i = 0; i < count; i++) {

        switch (nums[i]) {

            case 0:  term_fg = 0xFFFFFF; break;
            case 30: term_fg = 0x000000; break;
            case 31: term_fg = 0xFF3333; break;
            case 32: term_fg = 0x33FF33; break;
            case 33: term_fg = 0xFFFF33; break;
            case 34: term_fg = 0x3399FF; break;
            case 35: term_fg = 0xFF33FF; break;
            case 36: term_fg = 0x33FFFF; break;
            case 37: term_fg = 0xFFFFFF; break;

            case 38:
                if (i + 2 < count && nums[i + 1] == 5) {
                    term_fg = ansi_256_to_rgb(nums[i + 2]);
                    i += 2;
                }
                break;
        }
    }
}

void fb_putchar_cursor_utf8(char byte, uint32_t fg, uint32_t bg)
{
    (void)fg;
    (void)bg;

    if (esc_state) {
        if (esc_state == 1) {
            if (byte == '[') {
                esc_state = 2;
                esc_len = 0;
            } else {
                esc_state = 0;
            }
            return;
        }

        if (esc_state == 2) {
            if ((byte >= '0' && byte <= '9') || byte == ';') {
                if (esc_len < 15)
                    esc_buf[esc_len++] = byte;
                return;
            }

            if (byte == 'm') {
                handle_ansi();
                esc_state = 0;
                return;
            }

            esc_state = 0;
            return;
        }
    }

    if ((uint8_t)byte == 0x1B) {
        esc_state = 1;
        return;
    }

    uint8_t b = (uint8_t)byte;

    if (b < 0x80) {
        utf8_left = 0;
        fb_putchar_cursor(byte, 0, 0);
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
            if (utf8_left == 0) {
                char c = (utf8_cp < 128) ? (char)utf8_cp : '?';
                fb_putchar_cursor(c, 0, 0);
            }
        }
    }
}
