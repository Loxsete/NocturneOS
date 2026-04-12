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
#include <arch/x86_64/usermode.h>
#include <fs/ext2.h>

extern void user_shell(void);

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

extern uint8_t _binary_build_initramfs_cpio_start[];
extern uint8_t _binary_build_initramfs_cpio_end[];

pml4_t kernel_pml4;
static uint8_t user_stack[8192];

void vfs_setup(void *initramfs_data, uint64_t initramfs_size)
{
    uint64_t heap_phys = pmm_alloc_n(256);
    void    *heap_virt = (void *)(heap_phys + vmm_hhdm_offset());
    kmalloc_init(heap_virt, PAGE_SIZE * 256);
    vfs_init();
    vfs_node_t *root = tmpfs_create();
    vfs_mount("/", root);
    vfs_mkdir("/dev");
    vfs_mkdir("/proc");
    vfs_mkdir("/etc");
    vfs_mkdir("/bin");
    vfs_mkdir("/tmp");
    vfs_mkdir("/mnt");
    vfs_node_t *dev = vfs_resolve("/dev");
    if (dev && dev->ops && dev->ops->mkfile) {
        vfs_node_t *node = dev->ops->mkfile(dev, "console");
        if (node)
            console_init_node(node);
    }
    vfs_node_t *proc = procfs_create();
    vfs_mount("/proc", proc);
    procfs_register("mem", proc_mem_read);
    procfs_register("cpuinfo", proc_cpu_read);
    if (initramfs_data && initramfs_size > 0)
        initramfs_unpack(initramfs_data, initramfs_size, root);
}

static void mount_ext2(void)
{
    char buf[4] = "0";
    for (int bus = 0; bus < 2; bus++) {
        for (int drv = 0; drv < 2; drv++) {
            if (!ata_disks[bus][drv].present) continue;
            vfs_node_t *r = ext2_mount(bus, drv);
            if (r) {
                vfs_mount("/mnt", r);
                buf[0] = '0' + bus;
                kputs("ext2: mounted bus="); kputs(buf);
                buf[0] = '0' + drv;
                kputs(" drv="); kputs(buf);
                kputs("\n");
                return;
            }
        }
    }
    kputs("ext2:no ext2 disk found\n");
}

void _start(void)
{
    if (!fb_request.response || fb_request.response->framebuffer_count == 0)
        for (;;) __asm__ volatile("hlt");

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
    vfs_setup(
        _binary_build_initramfs_cpio_start,
        _binary_build_initramfs_cpio_end - _binary_build_initramfs_cpio_start
    );
    mount_ext2();
    kputs_col("lxtos kernel\n", COLOR_PROMPT);
    jump_usermode((uint64_t)user_shell,
                  (uint64_t)(user_stack + sizeof(user_stack)));
    for (;;) __asm__ volatile("hlt");
}
