#include <kernel/mm/vmm.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/kprintf.h>
#include <string.h>

/*
Kernel Heap implementation using a linked list of free blocks.
*/

#define PAGE_SIZE 4096
#define HEAP_START 0x1000000
#define HEAP_SIZE (PAGE_SIZE * 128)
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define MIN_BLOCK_SIZE (sizeof(free_block_t) + sizeof(void*))

typedef struct free_block {
    struct free_block *next;
    size_t size;
    uint32_t magic;  // Magic number to detect corruption
} free_block_t;

#define FREE_MAGIC 0xDEADBEEF
#define ALLOC_MAGIC 0xCAFEBABE

static uintptr_t heap_top = HEAP_START;
static free_block_t *free_list = NULL;

static void expand_heap(size_t size) {
    size = ALIGN_UP(size, PAGE_SIZE);
    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        uintptr_t vaddr = heap_top + i;
        uintptr_t paddr = vaddr;
        map_virtual_to_physical(vaddr, paddr, PAGE_PRESENT | PAGE_WRITABLE);
    }
    heap_top += size;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Add space for magic number and align
    size = ALIGN_UP(size + sizeof(uint32_t), sizeof(void *));
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE;

    free_block_t **prev = &free_list;
    for (free_block_t *block = free_list; block; block = block->next) {
        if (block->magic != FREE_MAGIC) {
            kprintf(ERROR, "[KMALLOC] Memory corruption detected at %p\n", block);
            return NULL;
        }
        if (block->size >= size) {
            *prev = block->next;
            // Mark as allocated
            block->magic = ALLOC_MAGIC;
            // Zero out the memory
            memset(block + 1, 0, size - sizeof(free_block_t));
            return (void *)(block + 1);
        }
        prev = &block->next;
    }

    if (heap_top + size > HEAP_START + HEAP_SIZE) {
        expand_heap(size);
    }

    free_block_t *block = (free_block_t*)heap_top;
    block->size = size;
    block->magic = ALLOC_MAGIC;
    block->next = NULL;
    
    heap_top += size;
    
    // Zero out the memory
    memset(block + 1, 0, size - sizeof(free_block_t));
    return (void *)(block + 1);
}

void kfree(void *ptr) {
    if (!ptr) return;

    free_block_t *block = (free_block_t *)ptr - 1;
    
    // Check for memory corruption
    if (block->magic != ALLOC_MAGIC) {
        kprintf(ERROR, "[KFREE] Memory corruption detected at %p\n", block);
        return;
    }
    
    // Mark as free
    block->magic = FREE_MAGIC;
    block->next = free_list;
    free_list = block;
    
    // Zero out the memory
    memset(ptr, 0, block->size - sizeof(free_block_t));
}
