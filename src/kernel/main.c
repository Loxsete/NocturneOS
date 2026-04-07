#include <stdint.h>
#include <stddef.h>

#include "limine.h"
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <console/shell.h>
#include <lib/kmalloc.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>
#include <fs/procfs.h>
#include <fs/initramfs.h>
#include <drivers/ata.h>
#include <drivers/console.h>
#include <console/kprint.h>

void draw_banner(void)
{
    fb_cursor_x = 0;
    fb_cursor_y = 0;
    kputs_col("lxtos kernel\n", COLOR_PROMPT);
    kputs("type 'help' for commands\n");
}


extern uint8_t _binary_build_initramfs_cpio_start[];
extern uint8_t _binary_build_initramfs_cpio_end[];

static uint8_t kernel_heap[4 * 1024 * 1024];
 
void vfs_setup(void *initramfs_data, uint64_t initramfs_size)
{
    kmalloc_init(kernel_heap, sizeof(kernel_heap));
    vfs_init();
     
    vfs_node_t *root = tmpfs_create();
    vfs_mount("/", root);
 
    vfs_mkdir("/dev");
    vfs_mkdir("/proc");
    vfs_mkdir("/etc");
    vfs_mkdir("/bin");
    vfs_mkdir("/tmp");
    

vfs_node_t *dev = vfs_resolve("/dev");

if (dev && dev->ops && dev->ops->mkfile) {
    vfs_node_t *node = dev->ops->mkfile(dev, "console");
    if (node) {
        console_init_node(node);
    }
}

    vfs_node_t *proc = procfs_create();
    vfs_mount("/proc", proc);
 
 
    if (initramfs_data && initramfs_size > 0)
        initramfs_unpack(initramfs_data, initramfs_size, root);
}

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

void _start(void)
{
    if (!fb_request.response || fb_request.response->framebuffer_count == 0)
        for (;;) __asm__ volatile("hlt");

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
    fb_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
    fb_fill(COLOR_BG);

    ata_init();
    kb_init();
    vfs_setup(
        _binary_build_initramfs_cpio_start,
        _binary_build_initramfs_cpio_end - _binary_build_initramfs_cpio_start
    );
    
    draw_banner();
    shell_run();

    for (;;) __asm__ volatile("hlt");
}
