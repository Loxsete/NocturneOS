#include <mm/pmm.h>
#include <lib/kstring.h>
#include <console/kprint.h>
#include <limine.h>

__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

static uint8_t  pmm_bitmap[131072]; // 1024 * 1024 / 8
static uint64_t pmm_total;
static uint64_t pmm_used;
static uint64_t pmm_total_ram = 0;

static void bitmap_set(uint64_t page)   { pmm_bitmap[page/8] |=  (1 << (page%8)); }
static void bitmap_clear(uint64_t page) { pmm_bitmap[page/8] &= ~(1 << (page%8)); }
static int  bitmap_test(uint64_t page)  { return (pmm_bitmap[page/8] >> (page%8)) & 1; }

void pmm_init(void)
{
    kmemset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));
    pmm_total = sizeof(pmm_bitmap) * 8;
    pmm_used  = pmm_total;

    if (!memmap_req.response) {
        kputs("pmm: no memmap from limine!\n");
        return;
    }

    struct limine_memmap_response *memmap = memmap_req.response;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *ent = memmap->entries[i];

        if (ent->type == LIMINE_MEMMAP_USABLE ||
            ent->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE ||
            ent->type == LIMINE_MEMMAP_KERNEL_AND_MODULES)
        {
            pmm_total_ram += ent->length;
        }

        if (ent->type != LIMINE_MEMMAP_USABLE) continue;

        uint64_t pages = ent->length / PAGE_SIZE;
        for (uint64_t pg = 0; pg < pages; pg++) {
            uint64_t idx = (ent->base / PAGE_SIZE) + pg;
            if (idx >= pmm_total) break;
            if (bitmap_test(idx)) { bitmap_clear(idx); pmm_used--; }
        }
    }

    kputs("pmm: ");
    kputdec((pmm_total - pmm_used) * PAGE_SIZE / (1024 * 1024));
    kputs(" MiB free / ");
    kputdec(pmm_total_ram / (1024 * 1024));
    kputs(" MiB total\n");
}

uint64_t pmm_alloc(void)
{
    for (uint64_t i = 0; i < pmm_total; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            pmm_used++;
            return i * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free(uint64_t phys)
{
    uint64_t i = phys / PAGE_SIZE;
    if (i >= pmm_total) return;
    if (bitmap_test(i)) { bitmap_clear(i); pmm_used--; }
}

uint64_t pmm_alloc_n(uint64_t n)
{
    for (uint64_t i = 0; i + n <= pmm_total; i++) {
        uint64_t j;
        for (j = 0; j < n; j++)
            if (bitmap_test(i + j)) break;
        if (j == n) {
            for (uint64_t k = 0; k < n; k++) { bitmap_set(i+k); pmm_used++; }
            return i * PAGE_SIZE;
        }
    }
    return 0;
}

uint64_t pmm_free_pages(void)   { return pmm_total - pmm_used; }
uint64_t pmm_total_pages(void)  { return pmm_total; }
uint64_t pmm_total_bytes(void)  { return pmm_total_ram; }
