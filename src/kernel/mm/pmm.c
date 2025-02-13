#include "kernel/mm/pmm.h"
#include "kernel/kprintf.h"

static uint64_t MAX_ORDER = 0;
static uint64_t MEM_SIZE = 0;

typedef struct Block {
    struct Block* next;
} Block;

static Block** free_lists;
static uint8_t* memory_pool;

void buddy_init(uintptr_t mem_start, uint64_t mem_size) {

    MEM_SIZE = mem_size;

    while ((1ull << MAX_ORDER) < mem_size) {
        MAX_ORDER++;
    }

    free_lists = (Block**) mem_start;
    memory_pool = (uint8_t*) mem_start + sizeof(Block*) * (MAX_ORDER + 1);

    for (size_t i = 0; i <= MAX_ORDER; ++i) {
        free_lists[i] = NULL;
    }

    free_lists[MAX_ORDER] = (Block*) memory_pool;
    free_lists[MAX_ORDER]->next = NULL;
}

void* buddy_alloc(size_t size) {

    if (size == 0 || size > MEM_SIZE) {
        kprintf(ERROR, "Invalid memory size\n");
        return NULL;
    }

    int order = 0;

    size_t block_size = 1;
    while (block_size < size) {
        block_size <<= 1;
        order++;
    }

    for (size_t i = order; i <= MAX_ORDER; ++i) {
        if (free_lists[i]) {
            Block* block = free_lists[i];
            free_lists[i] = block->next;

            for (int j = i; j > order; --j) {
                Block* buddy = (Block*)((uint8_t*)block + (1 << (j - 1)));
                buddy->next = free_lists[j - 1];
                free_lists[j - 1] = buddy;
            }
            return (void*)((uintptr_t)block + sizeof(Block));
        }
    } 

    kprintf(ERROR, "Memory insufficient\n");

    return NULL;
}

void buddy_free(void* ptr) {

    Block* block = (Block*)((uintptr_t)ptr - sizeof(Block));

    size_t order = 0;
    while ((uint8_t*) block - memory_pool >= (1 << order)) {
        order++;
    }

    block->next = free_lists[order];
    free_lists[order] = block;

    size_t block_size = (1 << order);
    while (order < MAX_ORDER) {
        Block* buddy = (Block*)((uintptr_t) block ^ block_size);
        if ((uint8_t*) buddy >= memory_pool + PAGE_SIZE || buddy->next == NULL) {
            break;
        }

        Block* prev = NULL;
        Block* curr = free_lists[order];
        while (curr && curr != buddy) {
            prev = curr;
            curr = curr->next;
        }

        if (curr) {
            if (prev) {
                prev->next = curr->next;
            } else {
                free_lists[order] = curr->next;
            }

            if (block < buddy) {
                block = (Block*) block;
            }
            block_size <<= 1;
            order++;
        } else {
            break;
        }
    }
}
