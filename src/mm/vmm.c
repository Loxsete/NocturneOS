#include <mm/vmm.h>
#include <mm/pmm.h>
#include <lib/kstring.h>
#include <console/kprint.h>
#include <limine.h>

// HHDM offset from limine
__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request vmm_hhdm_req = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

// Kernel address
__attribute__((used, section(".requests")))
static volatile struct limine_kernel_address_request kaddr_req = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
};

static inline uint64_t *phys_to_virt(uint64_t phys)
{
    return (uint64_t *)(phys + vmm_hhdm_req.response->offset);
}

// one table 4096 byte = 512 writes for 8 bytes
static uint64_t alloc_table(void)
{
    uint64_t phys = pmm_alloc();
    if (!phys) {
        kputs("vmm: oom allocating page table\n");
        for (;;) __asm__ volatile("hlt");
    }
    kmemset(phys_to_virt(phys), 0, PAGE_SIZE);
    return phys;
}

static uint64_t *get_or_create(uint64_t *table, uint64_t idx, uint64_t flags)
{
    if (!(table[idx] & PAGE_PRESENT)) {
        uint64_t phys = alloc_table();
        table[idx] = phys | flags;
    }
    return phys_to_virt(table[idx] & ~0xFFFULL);
}

uint64_t vmm_hhdm_offset(void)
{
    return vmm_hhdm_req.response->offset;
}

void vmm_map(pml4_t pml4, uint64_t virt, uint64_t phys, uint64_t flags)
{
    uint64_t i4 = (virt >> 39) & 0x1FF;
    uint64_t i3 = (virt >> 30) & 0x1FF;
    uint64_t i2 = (virt >> 21) & 0x1FF;
    uint64_t i1 = (virt >> 12) & 0x1FF;

    uint64_t *pdpt = get_or_create(pml4,  i4, flags | PAGE_PRESENT);
    uint64_t *pd   = get_or_create(pdpt,  i3, flags | PAGE_PRESENT);
    uint64_t *pt   = get_or_create(pd,    i2, flags | PAGE_PRESENT);

    pt[i1] = (phys & ~0xFFFULL) | flags;
}

void vmm_unmap(pml4_t pml4, uint64_t virt)
{
    uint64_t i4 = (virt >> 39) & 0x1FF;
    uint64_t i3 = (virt >> 30) & 0x1FF;
    uint64_t i2 = (virt >> 21) & 0x1FF;
    uint64_t i1 = (virt >> 12) & 0x1FF;

    if (!(pml4[i4] & PAGE_PRESENT)) return;
    uint64_t *pdpt = phys_to_virt(pml4[i4] & ~0xFFFULL);

    if (!(pdpt[i3] & PAGE_PRESENT)) return;
    uint64_t *pd = phys_to_virt(pdpt[i3] & ~0xFFFULL);

    if (!(pd[i2] & PAGE_PRESENT)) return;
    uint64_t *pt = phys_to_virt(pd[i2] & ~0xFFFULL);

    pt[i1] = 0;

    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}


void vmm_switch(pml4_t pml4)
{

    uint64_t phys = (uint64_t)pml4 - vmm_hhdm_req.response->offset;
    __asm__ volatile("mov %0, %%cr3" :: "r"(phys) : "memory");
}


pml4_t vmm_new_space(void)
{
    uint64_t phys = alloc_table();
    uint64_t *new_pml4 = phys_to_virt(phys);

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *cur_pml4 = phys_to_virt(cr3 & ~0xFFFULL);

    for (int i = 256; i < 512; i++)
        new_pml4[i] = cur_pml4[i];

    return new_pml4;
}


pml4_t vmm_init(void)
{
    if (!vmm_hhdm_req.response || !kaddr_req.response) {
        kputs("vmm: missing limine responses\n");
        for (;;) __asm__ volatile("hlt");
    }

    uint64_t hhdm   = vmm_hhdm_req.response->offset;
    uint64_t k_phys = kaddr_req.response->physical_base;
    uint64_t k_virt = kaddr_req.response->virtual_base;

    pml4_t pml4 = (pml4_t)phys_to_virt(alloc_table());

    for (uint64_t p = 0; p < 0x100000000ULL; p += PAGE_SIZE)
        vmm_map(pml4, hhdm + p, p, KERNEL_FLAGS);

    for (uint64_t off = 0; off < 0x4000000ULL; off += PAGE_SIZE)
        vmm_map(pml4, k_virt + off, k_phys + off, USER_FLAGS);

    vmm_switch(pml4);
    kputs("vmm: init done\n");

    return pml4;
}
