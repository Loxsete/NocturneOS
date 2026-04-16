#include <arch/x86_64/gdt.h>
#include <lib/kstring.h>

extern void gdt_load(struct gdt_ptr *ptr);
extern void tss_load(uint16_t selector);
extern void gdt_flush_segments(void);

static struct gdt_entry gdt[7];
static struct gdt_ptr   gdtp;

struct tss       kernel_tss;

static void gdt_set(int i, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran)
{
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].access      = access;
    gdt[i].granularity = ((limit >> 16) & 0x0F);
    gdt[i].granularity |= gran & 0xF0;
    gdt[i].base_high   = (base >> 24) & 0xFF;
}

static void gdt_set_tss(int i, struct tss *tss)
{
    uint64_t base = (uint64_t)tss;
    uint64_t limit = sizeof(struct tss);

    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;

    gdt[i].access      = 0x89;
    gdt[i].granularity = (limit >> 16) & 0x0F;
    gdt[i].granularity |= 0x00;

    gdt[i].base_high   = (base >> 24) & 0xFF;

    *(uint32_t *)&gdt[i + 1] = (base >> 32);
    *((uint32_t *)&gdt[i + 1] + 1) = 0;
}

void gdt_init(void)
{
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint64_t)&gdt;

    gdt_set(0, 0, 0, 0, 0);

    // ring0
    gdt_set(1, 0, 0x000FFFFF,
        0x9A, 0xA0);

    // kernel data
    gdt_set(2, 0, 0x000FFFFF,
        0x92, 0xA0);

    // ring3
    gdt_set(3, 0, 0x000FFFFF,
        0xF2, 0xA0);

    // ring3
    gdt_set(4, 0, 0x000FFFFF,
        0xFA, 0xA0);

    // TSS
    kmemset(&kernel_tss, 0, sizeof(kernel_tss));
    kernel_tss.rsp0 = 0; // todo sete

    gdt_set_tss(5, &kernel_tss);

    gdt_load(&gdtp);
	gdt_flush_segments();
    tss_load(0x28);
}
