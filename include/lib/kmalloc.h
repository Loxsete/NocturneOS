#pragma once
#include <stdint.h>
#include <stddef.h>

// Kernel memory allocator
void kmalloc_init(void *heap_start, uint64_t heap_size);

// Allocate size bytes of memory
void *kmalloc(uint64_t size);

// Allocate size zeroed bytes of memory
void *kzalloc(uint64_t size);

// Free allocated memory
void kfree(void *ptr);

// Get the amount of free memory
uint64_t kmalloc_free_size(void);

// Get the total heap size
uint64_t kmalloc_heap_size(void);
