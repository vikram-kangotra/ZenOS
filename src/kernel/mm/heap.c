#include "kernel/mm/vmm.h"
#include "kermel/mm/heap.h"
#include "kernel/kprintf.h"

#define PAGE_SIZE 4096
#define HEAP_START 0xFFFF800000000000
#define HEAP_SIZE (PAGE_SIZE * 128)
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

typedef struct free_block {
    struct free_block *next;
    size_t size;
} free_block_t;

static uintptr_t heap_top = HEAP_START;
static free_block_t *free_list = NULL;

static void expand_heap(size_t size) {
    size = ALIGN_UP(size, PAGE_SIZE);
    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        uintptr_t vaddr = heap_top + i;
        uintptr_t paddr = vaddr - HEAP_START;
        map_virtual_to_physical(vaddr, paddr);
    }
    heap_top += size;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    size = ALIGN_UP(size, sizeof(void *));

    free_block_t **prev = &free_list;

    for (free_block_t *block = free_list; block; block = block->next) {
        if (block->size >= size) {
            *prev = block->next;
            return (void *)(block + 1);
        }
        prev = &block->next;
    }

    if (heap_top + size > HEAP_START + HEAP_SIZE) {
        expand_heap(size);
    }

    void *ptr = (void*) heap_top;
    heap_top += size;
    return ptr;
}
