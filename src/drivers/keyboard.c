#include <drivers/keyboard.h>
#include <drivers/framebuffer.h>
#include <stdint.h>
#include <arch/x86_64/io.h>

#define KB_DATA    0x60
#define KB_STATUS  0x64

#define SC_LSHIFT   0x2A
#define SC_RSHIFT   0x36
#define SC_LSHIFT_R 0xAA
#define SC_RSHIFT_R 0xB6
#define SC_LCTRL    0x1D
#define SC_LCTRL_R  0x9D

static const char scancode_map[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q',  'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char scancode_map_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A',  'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|',  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int shift_held = 0;
static int ctrl_held  = 0;

void kb_init(void)
{
    while (inb(KB_STATUS) & 0x01)
        inb(KB_DATA);
}

int kb_has_key(void)
{
    return inb(KB_STATUS) & 0x01;
}

char kb_getchar(void)
{
    while (1) {
        while (!kb_has_key());
        uint8_t sc = inb(KB_DATA);

        if (sc == SC_LSHIFT || sc == SC_RSHIFT)   { shift_held = 1; continue; }
        if (sc == SC_LSHIFT_R || sc == SC_RSHIFT_R) { shift_held = 0; continue; }
        if (sc == SC_LCTRL)   { ctrl_held = 1; continue; }
        if (sc == SC_LCTRL_R) { ctrl_held = 0; continue; }
        if (sc & 0x80) continue;
        if (sc >= 128) continue;

        char c = shift_held ? scancode_map_shift[sc] : scancode_map[sc];
        if (!c) continue;

        if (ctrl_held) {
            if (c == '=' || c == '+') {
                if (fb_font_scale < 4) fb_font_scale++;
                fb_fill(COLOR_BG);
                fb_cursor_x = 0;
                fb_cursor_y = 0;
                continue;
            }
            if (c == '-') {
                if (fb_font_scale > 1) fb_font_scale--;
                fb_fill(COLOR_BG);
                fb_cursor_x = 0;
                fb_cursor_y = 0;
                continue;
            }
        }

        return c;
    }
}
