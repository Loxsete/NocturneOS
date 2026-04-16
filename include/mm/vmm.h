#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITE    (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_NX       (1ULL << 63)

#define KERNEL_FLAGS  (PAGE_PRESENT | PAGE_WRITE)
#define USER_FLAGS    (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

typedef uint64_t *pml4_t;


// Get the offset of the HHDM
uint64_t vmm_hhdm_offset(void);

// Map a virtual address to a physical address
void vmm_map(pml4_t pml4, uint64_t virt, uint64_t phys, uint64_t flags);

// Unmap a virtual address
void vmm_unmap(pml4_t pml4, uint64_t virt);

// Switch to a new page table
void vmm_switch(pml4_t pml4);

// Create a new address space
pml4_t vmm_new_space(void);

// Initialize the virtual memory manager
pml4_t vmm_init(void);
