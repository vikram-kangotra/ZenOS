#include "kernel/mm/pmm.h"
#include "kernel/kprintf.h"
#include "kernel/sync.h"

#define MIN_ORDER 12  // 4KB minimum block size
#define MAX_ORDER 20  // 1MB maximum block size
#define BLOCK_MAGIC 0xCAFEBABE
#define PAGE_SHIFT 12  // 4KB pages
#define PAGE_SIZE (1 << PAGE_SHIFT)

typedef struct Block {
    struct Block* next;
    uint64_t order;    // Size of this block (2^order bytes)
    uint32_t magic;    // Magic number for corruption detection
} Block;

static Block** free_lists;
static uint8_t* memory_pool;
static uint64_t total_memory;
static mutex_t buddy_mutex;
static uint64_t total_pages = 0;
static uint64_t used_pages = 0;

void buddy_init(uintptr_t mem_start, uint64_t mem_size) {
    mutex_init(&buddy_mutex, "buddy_mutex");
    
    total_memory = mem_size;
    total_pages = mem_size / PAGE_SIZE;
    used_pages = 0;
    
    // Initialize free lists
    free_lists = (Block**)mem_start;
    for (uint64_t i = 0; i <= MAX_ORDER; i++) {
        free_lists[i] = NULL;
    }
    
    // Calculate memory pool start (aligned to MAX_ORDER)
    uint64_t pool_start = (uint64_t)mem_start + sizeof(Block*) * (MAX_ORDER + 1);
    pool_start = (pool_start + (1ull << MAX_ORDER) - 1) & ~((1ull << MAX_ORDER) - 1);
    memory_pool = (uint8_t*)pool_start;
    
    // Initialize the initial free block
    uint64_t initial_order = MAX_ORDER;
    while ((1ull << initial_order) > mem_size) {
        initial_order--;
    }
    
    Block* initial_block = (Block*)memory_pool;
    initial_block->next = NULL;
    initial_block->order = initial_order;
    initial_block->magic = BLOCK_MAGIC;
    
    free_lists[initial_order] = initial_block;
    
    kprintf(INFO, "Buddy allocator initialized with %d MB of memory\n", mem_size >> 20);
}

void* buddy_alloc(size_t size) {
    if (size == 0 || size > (1ull << MAX_ORDER)) {
        kprintf(ERROR, "Invalid allocation size: %zu\n", size);
        return NULL;
    }
    
    mutex_acquire(&buddy_mutex);
    
    // Calculate required order
    uint64_t order = MIN_ORDER;
    while ((1ull << order) < size + sizeof(Block)) {
        order++;
    }
    
    // Find a suitable block
    uint64_t current_order = order;
    while (current_order <= MAX_ORDER && !free_lists[current_order]) {
        current_order++;
    }
    
    if (current_order > MAX_ORDER) {
        kprintf(ERROR, "No suitable block found for size %zu\n", size);
        mutex_release(&buddy_mutex);
        return NULL;
    }
    
    // Split blocks until we get the right size
    Block* block = free_lists[current_order];
    free_lists[current_order] = block->next;
    
    while (current_order > order) {
        current_order--;
        
        // Create buddy block
        Block* buddy = (Block*)((uint8_t*)block + (1ull << current_order));
        buddy->next = free_lists[current_order];
        buddy->order = current_order;
        buddy->magic = BLOCK_MAGIC;
        free_lists[current_order] = buddy;
        
        // Update current block
        block->order = current_order;
    }
    
    // Mark block as allocated
    block->magic = ~BLOCK_MAGIC;
    
    used_pages += (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    mutex_release(&buddy_mutex);
    return (void*)((uint8_t*)block + sizeof(Block));
}

void buddy_free(void* ptr) {
    if (!ptr) return;
    
    mutex_acquire(&buddy_mutex);
    
    Block* block = (Block*)((uint8_t*)ptr - sizeof(Block));
    
    // Check for corruption
    if (block->magic != ~BLOCK_MAGIC) {
        kprintf(ERROR, "Invalid block magic: %x\n", block->magic);
        mutex_release(&buddy_mutex);
        return;
    }
    
    uint64_t order = block->order;
    used_pages -= (1ULL << (order - PAGE_SHIFT));  // Calculate size from order
    
    // Try to merge with buddy blocks
    while (order < MAX_ORDER) {
        uint64_t block_size = 1ull << order;
        uint64_t buddy_addr = (uint64_t)block ^ block_size;
        
        // Check if buddy is in our memory pool
        if (buddy_addr < (uint64_t)memory_pool || 
            buddy_addr >= (uint64_t)memory_pool + total_memory) {
            break;
        }
        
        Block* buddy = (Block*)buddy_addr;
        
        // Check if buddy is free and same order
        if (buddy->magic != BLOCK_MAGIC || buddy->order != order) {
            break;
        }
        
        // Remove buddy from free list
        Block* prev = NULL;
        Block* curr = free_lists[order];
        while (curr && curr != buddy) {
            prev = curr;
            curr = curr->next;
        }
        
        if (!curr) break;
        
        if (prev) {
            prev->next = curr->next;
        } else {
            free_lists[order] = curr->next;
        }
        
        // Merge blocks
        if (block > buddy) {
            block = buddy;
        }
        order++;
        block->order = order;
    }
    
    // Add block to free list
    block->magic = BLOCK_MAGIC;
    block->next = free_lists[order];
    free_lists[order] = block;
    
    mutex_release(&buddy_mutex);
}

uint64_t get_free_ram(void) {
    uint64_t free_bytes = 0;
    for (int i = 0; i <= MAX_ORDER; i++) {
        Block* block = free_lists[i];
        while (block) {
            free_bytes += (1ULL << (i + PAGE_SHIFT));
            block = block->next;
        }
    }
    return free_bytes / 1024;  // Convert bytes to KB
}

uint64_t get_used_ram(void) {
    return (used_pages * PAGE_SIZE) / 1024;  // Convert bytes to KB
}

uint64_t get_fragmented_ram(void) {
    uint64_t fragmented_bytes = 0;
    for (int i = 0; i <= MAX_ORDER; i++) {
        Block* block = free_lists[i];
        if (block) {
            fragmented_bytes += (1ULL << (i + PAGE_SHIFT));
        }
    }
    return fragmented_bytes / 1024;  // Convert bytes to KB
}

void test_buddy_allocator(void) {
    kprintf(INFO, "Starting buddy allocator tests...\n");
    
    // Test 1: Basic allocation and free
    kprintf(INFO, "Test 1: Basic allocation and free\n");
    void* ptr1 = buddy_alloc(4096);  // 4KB
    if (ptr1) {
        kprintf(INFO, "Allocated 4KB at %p\n", ptr1);
        buddy_free(ptr1);
        kprintf(INFO, "Freed 4KB block\n");
    }
    
    // Test 2: Multiple allocations
    kprintf(INFO, "Test 2: Multiple allocations\n");
    void* ptr2 = buddy_alloc(8192);  // 8KB
    void* ptr3 = buddy_alloc(16384); // 16KB
    if (ptr2 && ptr3) {
        kprintf(INFO, "Allocated 8KB at %p and 16KB at %p\n", ptr2, ptr3);
        buddy_free(ptr2);
        buddy_free(ptr3);
        kprintf(INFO, "Freed both blocks\n");
    }
    
    // Test 3: Edge cases
    kprintf(INFO, "Test 3: Edge cases\n");
    void* ptr4 = buddy_alloc(1);     // Minimum size
    void* ptr5 = buddy_alloc(1 << 20); // 1MB
    if (ptr4 && ptr5) {
        kprintf(INFO, "Allocated 1B at %p and 1MB at %p\n", ptr4, ptr5);
        buddy_free(ptr4);
        buddy_free(ptr5);
        kprintf(INFO, "Freed both blocks\n");
    }
    
    // Test 4: Invalid allocations
    kprintf(INFO, "Test 4: Invalid allocations\n");
    void* ptr6 = buddy_alloc(0);     // Zero size
    void* ptr7 = buddy_alloc(1 << 21); // Too large (2MB)
    if (!ptr6 && !ptr7) {
        kprintf(INFO, "Correctly rejected invalid allocations\n");
    }
    
    // Test 6: Stress test
    kprintf(INFO, "Test 6: Stress test\n");
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = buddy_alloc(4096 * (i + 1));
        if (ptrs[i]) {
            kprintf(INFO, "Allocated %d KB at %p\n", 4 * (i + 1), ptrs[i]);
        }
    }
    
    // Free in reverse order
    for (int i = 9; i >= 0; i--) {
        if (ptrs[i]) {
            buddy_free(ptrs[i]);
            kprintf(INFO, "Freed %d KB block\n", 4 * (i + 1));
        }
    }
    
    kprintf(INFO, "Buddy allocator tests completed\n");
    kprintf(INFO, "Final memory statistics:\n");
    kprintf(INFO, "Free RAM: %llu KB\n", get_free_ram());
    kprintf(INFO, "Used RAM: %llu KB\n", get_used_ram());
    kprintf(INFO, "Fragmented RAM: %llu KB\n", get_fragmented_ram());
}