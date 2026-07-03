#include <stdint.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <lib/kmalloc.h>
#include <fs/vfs.h>
#include <fs/initramfs.h>
#include <fs/tmpfs.h>

void early_vfs_init(void *initramfs_data, uint64_t initramfs_size)
{
    uint64_t heap_phys = pmm_alloc_n(256);
    void    *heap_virt = (void *)(heap_phys + vmm_hhdm_offset());
    kmalloc_init(heap_virt, PAGE_SIZE * 256);

    vfs_init();
    vfs_node_t *root = tmpfs_create();
    vfs_mount("/", root);

    // this is minimum, to load init
    vfs_mkdir("/bin");
    vfs_mkdir("/tmp");
    vfs_mkdir("/etc");

    if (initramfs_data && initramfs_size > 0)
            initramfs_unpack(initramfs_data, initramfs_size, root);
}
