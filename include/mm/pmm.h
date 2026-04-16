#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE  4096ULL
#define PAGE_SHIFT 12


// Initialize the physical memory manager
void pmm_init(void);

// Allocate a single physical page
uint64_t pmm_alloc(void);

// Free a previously allocated physical page
void pmm_free(uint64_t phys);

// Get the number of free physical pages
uint64_t pmm_free_pages(void);

// Get the total number of physical pages
uint64_t pmm_total_pages(void);

// Allocate multiple contiguous physical pages
uint64_t pmm_alloc_n(uint64_t n);

// Get the total number of physical bytes
uint64_t pmm_total_bytes(void);
