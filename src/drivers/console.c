#include <fs/vfs.h>
#include <drivers/keyboard.h>
#include <drivers/framebuffer.h>

static int64_t console_read(vfs_node_t *node, void *buf, uint64_t offset, uint64_t size)
{
    (void)node; (void)offset;

    char *out = (char *)buf;
    for (uint64_t i = 0; i < size; i++) {
        char c = kb_getchar();
        if (!c) return i;

        out[i] = c;

        if (c == '\n')
            return i + 1;
    }
    return size;
}

static int64_t console_write(vfs_node_t *node, const void *buf, uint64_t offset, uint64_t size)
{
    (void)node; (void)offset;

    const char *in = (const char *)buf;

    if (size >= 4 &&
        in[0] == '\x1b' &&
        in[1] == '[' &&
        in[2] == '2' &&
        in[3] == 'J')
    {
        fb_fill(COLOR_BG);
        fb_cursor_x = 0;
        fb_cursor_y = 0;
        return size;
    }

    for (uint64_t i = 0; i < size; i++)
        fb_putchar_cursor(in[i], COLOR_FG, COLOR_BG);

    return size;
}

vfs_ops_t console_ops = {
    .read  = console_read,
    .write = console_write,
};

void console_init_node(vfs_node_t *node)
{
    node->flags = VFS_FLAG_FILE;
    node->ops   = &console_ops;
}