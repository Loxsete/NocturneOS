#include <lib/kmalloc.h>


#define ALIGN8(x) (((x) + 7) & ~7)

typedef struct block_header {
    uint64_t            size;
    int                 free;
    struct block_header *next;
} block_header_t;

static uint8_t         *heap_base = NULL;
static uint64_t         heap_total = 0;
static block_header_t  *heap_head = NULL;

void kmalloc_init(void *start, uint64_t size)
{
    heap_base  = (uint8_t *)start;
    heap_total = size;

    heap_head        = (block_header_t *)heap_base;
    heap_head->size  = size - sizeof(block_header_t);
    heap_head->free  = 1;
    heap_head->next  = NULL;
}

static block_header_t *find_free(uint64_t size)
{
    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) return cur;
        cur = cur->next;
    }
    return NULL;
}

void *kmalloc(uint64_t size)
{
    if (!heap_base) return NULL;
    size = ALIGN8(size);

    block_header_t *blk = find_free(size);
    if (!blk) return NULL;

    if (blk->size >= size + sizeof(block_header_t) + 8) {
        block_header_t *split = (block_header_t *)((uint8_t *)blk + sizeof(block_header_t) + size);
        split->size = blk->size - size - sizeof(block_header_t);
        split->free = 1;
        split->next = blk->next;
        blk->next   = split;
        blk->size   = size;
    }

    blk->free = 0;
    return (void *)((uint8_t *)blk + sizeof(block_header_t));
}

void *kzalloc(uint64_t size)
{
    void *ptr = kmalloc(size);
    if (!ptr) return NULL;
    uint8_t *p = (uint8_t *)ptr;
    for (uint64_t i = 0; i < size; i++) p[i] = 0;
    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr) return;
    block_header_t *blk = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    blk->free = 1;

    block_header_t *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(block_header_t) + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

uint64_t kmalloc_free_size(void) {
    uint64_t free = 0;
    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free) free += cur->size;
        cur = cur->next;
    }
    return free;
}

uint64_t kmalloc_heap_size(void) {
    return heap_total;
}