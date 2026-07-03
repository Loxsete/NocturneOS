#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <lib/kmalloc.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>
#include <fs/procfs.h>
#include <fs/initramfs.h>
#include <drivers/ata.h>
#include <drivers/console.h>
#include <console/kprint.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/tss.h>
#include <exec/elf.h>
#include <fs/ext2.h>
#include <kernel/fd.h>
#include <kernel/init.h>

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

extern uint8_t _binary_build_initramfs_cpio_start[];
extern uint8_t _binary_build_initramfs_cpio_end[];

pml4_t kernel_pml4;

void _start(void)
{
    if (!fb_request.response || fb_request.response->framebuffer_count == 0)
            for (;;)
                    __asm__ volatile("hlt");

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
    fb_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
    fb_fill(COLOR_BG);

    pmm_init();
    kernel_pml4 = vmm_init();
    gdt_init();
    tss_init();
    idt_init();
    __asm__ volatile("sti");

    ata_init();
    kb_init();
	fd_table_init();
	
    early_vfs_init(_binary_build_initramfs_cpio_start,
                   _binary_build_initramfs_cpio_end - _binary_build_initramfs_cpio_start);

    kputs("[kernel] starting /bin/init\n");
    if (elf_exec("/bin/init") != 0)
            kputs("FATAL: cannot exec /bin/init\n");

    for (;;)
            __asm__ volatile("hlt");
}
