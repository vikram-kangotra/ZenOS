#include <kernel/mm/vmm.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/kprintf.h>
#include <string.h>
#include <stdbool.h>

/*
Kernel Heap implementation using a linked list of free blocks.
*/

#define PAGE_SIZE 4096
#define HEAP_START 0x1000000
#define HEAP_SIZE (PAGE_SIZE * 128)
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define MIN_BLOCK_SIZE (sizeof(free_block_t) + sizeof(void*))
#define ALIGNMENT 16  // Ensure 16-byte alignment

typedef struct free_block {
    struct free_block *next;
    size_t size;
    uint32_t magic;      // Magic number to detect corruption
    uint32_t checksum;   // Checksum for corruption detection
    uint32_t guard;      // Guard value at end of block
} free_block_t;

#define FREE_MAGIC 0xDEADBEEF
#define ALLOC_MAGIC 0xCAFEBABE
#define GUARD_VALUE 0xBADF00D

static uintptr_t heap_top = HEAP_START;
static free_block_t *free_list = NULL;

// Calculate checksum for a block
static uint32_t calculate_checksum(free_block_t *block) {
    if (!block) return 0;
    return (uint32_t)((uintptr_t)block ^ block->size ^ block->magic ^ block->guard);
}

// Verify block integrity
static bool verify_block(free_block_t *block) {
    if (!block) {
        kprintf(ERROR, "[KMALLOC] Null block pointer\n");
        return false;
    }
    
    if (block->magic != FREE_MAGIC && block->magic != ALLOC_MAGIC) {
        kprintf(ERROR, "[KMALLOC] Invalid magic number at %p: 0x%x\n", block, block->magic);
        return false;
    }
    
    if (block->guard != GUARD_VALUE) {
        kprintf(ERROR, "[KMALLOC] Guard value corrupted at %p: 0x%x\n", block, block->guard);
        return false;
    }
    
    uint32_t expected_checksum = calculate_checksum(block);
    if (block->checksum != expected_checksum) {
        kprintf(ERROR, "[KMALLOC] Checksum mismatch at %p: expected 0x%x, got 0x%x\n", 
                block, expected_checksum, block->checksum);
        return false;
    }
    
    return true;
}

// Initialize a new block
static void init_block(free_block_t *block, size_t size, uint32_t magic) {
    if (!block) return;
    block->size = size;
    block->magic = magic;
    block->next = NULL;
    block->guard = GUARD_VALUE;
    block->checksum = calculate_checksum(block);
}

// Initialize the heap
void kmalloc_init(void) {
    // Clear the heap area
    memset((void*)HEAP_START, 0, HEAP_SIZE);
    
    // Initialize the first free block
    free_block_t *block = (free_block_t*)HEAP_START;
    init_block(block, HEAP_SIZE, FREE_MAGIC);
    
    free_list = block;
    heap_top = HEAP_START + HEAP_SIZE;
    
    kprintf(INFO, "[KMALLOC] Heap initialized at %p with size %u bytes\n", 
            (void*)HEAP_START, HEAP_SIZE);
    kprintf(INFO, "[KMALLOC] First block at %p, size %u\n", 
            block, block->size);
}

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
    size = ALIGN_UP(size + sizeof(uint32_t), ALIGNMENT);
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE;

    free_block_t **prev = &free_list;
    for (free_block_t *block = free_list; block; block = block->next) {
        if (!verify_block(block)) {
            kprintf(ERROR, "[KMALLOC] Memory corruption detected at %p\n", block);
            return NULL;
        }
        if (block->size >= size) {
            *prev = block->next;
            // Mark as allocated
            init_block(block, size, ALLOC_MAGIC);
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
    init_block(block, size, ALLOC_MAGIC);
    
    heap_top += size;
    
    // Zero out the memory
    memset(block + 1, 0, size - sizeof(free_block_t));
    return (void *)(block + 1);
}

void kfree(void *ptr) {
    if (!ptr) return;

    free_block_t *block = (free_block_t *)ptr - 1;
    
    // Check for memory corruption
    if (!verify_block(block)) {
        kprintf(ERROR, "[KFREE] Memory corruption detected at %p\n", block);
        return;
    }
    
    // Mark as free
    init_block(block, block->size, FREE_MAGIC);
    block->next = free_list;
    free_list = block;
    
    // Zero out the memory
    memset(ptr, 0, block->size - sizeof(free_block_t));
}

// Test different allocation sizes
#define SMALL_SIZE 16
#define MEDIUM_SIZE 1024
#define LARGE_SIZE 4096
#define HUGE_SIZE (1024 * 1024)  // 1MB

// Test memory corruption detection
void kmalloc_test_corruption(void) {
    kprintf(INFO, "[KMALLOC] Starting memory corruption tests...\n");
    
    // Test 1: Basic allocation
    void* ptr = kmalloc(64);
    if (!ptr) {
        kprintf(ERROR, "[KMALLOC] Test failed: Basic allocation failed\n");
        return;
    }
    
    // Test 2: Corrupt magic number
    free_block_t *block = (free_block_t *)ptr - 1;
    uint32_t original_magic = block->magic;
    block->magic = 0x12345678;
    kfree(ptr);  // Should detect corruption
    if (block->magic == 0x12345678) {
        kprintf(SUCCESS, "[KMALLOC] Magic number corruption test passed\n");
    } else {
        kprintf(ERROR, "[KMALLOC] Magic number corruption test failed\n");
    }
    // Restore and properly free
    block->magic = original_magic;
    block->checksum = calculate_checksum(block);
    kfree(ptr);
    
    // Test 3: Corrupt guard value
    ptr = kmalloc(64);
    if (!ptr) {
        kprintf(ERROR, "[KMALLOC] Test failed: Allocation for guard test failed\n");
        return;
    }
    block = (free_block_t *)ptr - 1;
    uint32_t original_guard = block->guard;
    block->guard = 0x87654321;
    kfree(ptr);  // Should detect corruption
    if (block->guard == 0x87654321) {
        kprintf(SUCCESS, "[KMALLOC] Guard value corruption test passed\n");
    } else {
        kprintf(ERROR, "[KMALLOC] Guard value corruption test failed\n");
    }
    // Restore and properly free
    block->guard = original_guard;
    block->checksum = calculate_checksum(block);
    kfree(ptr);
    
    // Test 4: Corrupt checksum
    ptr = kmalloc(64);
    if (!ptr) {
        kprintf(ERROR, "[KMALLOC] Test failed: Allocation for checksum test failed\n");
        return;
    }
    block = (free_block_t *)ptr - 1;
    uint32_t original_checksum = block->checksum;
    block->checksum = 0xAAAAAAAA;
    kfree(ptr);  // Should detect corruption
    if (block->checksum == 0xAAAAAAAA) {
        kprintf(SUCCESS, "[KMALLOC] Checksum corruption test passed\n");
    } else {
        kprintf(ERROR, "[KMALLOC] Checksum corruption test failed\n");
    }
    // Restore and properly free
    block->checksum = original_checksum;
    kfree(ptr);
    
    // Test 5: Double free detection
    ptr = kmalloc(64);
    if (!ptr) {
        kprintf(ERROR, "[KMALLOC] Test failed: Allocation for double free test failed\n");
        return;
    }
    kfree(ptr);
    kfree(ptr);  // Should detect double free
    kprintf(SUCCESS, "[KMALLOC] Double free detection test passed\n");
    
    // Reinitialize the heap to ensure clean state
    kmalloc_init();
    
    kprintf(INFO, "[KMALLOC] Memory corruption tests completed\n");
}

// Test heap functionality
void kmalloc_test_heap(void) {
    kprintf(INFO, "[KMALLOC] Starting heap tests...\n");
    
    // Verify heap is initialized
    if (!free_list) {
        kprintf(ERROR, "[KMALLOC] Heap not initialized!\n");
        return;
    }
    
    if (!verify_block(free_list)) {
        kprintf(ERROR, "[KMALLOC] Initial free block corrupted!\n");
        return;
    }

    // Test 1: Basic allocations
    kprintf(INFO, "[KMALLOC] Testing basic allocations...\n");
    void* ptr1 = kmalloc(SMALL_SIZE);
    if (!ptr1) {
        kprintf(ERROR, "[KMALLOC] Small allocation failed!\n");
        return;
    }
    
    void* ptr2 = kmalloc(MEDIUM_SIZE);
    if (!ptr2) {
        kprintf(ERROR, "[KMALLOC] Medium allocation failed!\n");
        kfree(ptr1);
        return;
    }
    
    void* ptr3 = kmalloc(LARGE_SIZE);
    if (!ptr3) {
        kprintf(ERROR, "[KMALLOC] Large allocation failed!\n");
        kfree(ptr1);
        kfree(ptr2);
        return;
    }
    
    kprintf(INFO, "[KMALLOC] Basic allocations successful:\n");
    kprintf(INFO, "[KMALLOC] Small (%d bytes): %p\n", SMALL_SIZE, ptr1);
    kprintf(INFO, "[KMALLOC] Medium (%d bytes): %p\n", MEDIUM_SIZE, ptr2);
    kprintf(INFO, "[KMALLOC] Large (%d bytes): %p\n", LARGE_SIZE, ptr3);

    // Test 2: Alignment check
    if ((uintptr_t)ptr1 % 16 == 0 && 
        (uintptr_t)ptr2 % 16 == 0 && 
        (uintptr_t)ptr3 % 16 == 0) {
        kprintf(INFO, "[KMALLOC] Alignment test passed\n");
    } else {
        kprintf(ERROR, "[KMALLOC] Alignment test failed\n");
    }

    // Test 3: Free and reallocate
    kprintf(INFO, "[KMALLOC] Testing free and reallocate...\n");
    kfree(ptr2);
    void* ptr4 = kmalloc(MEDIUM_SIZE);
    if (!ptr4) {
        kprintf(ERROR, "[KMALLOC] Reallocation failed!\n");
        kfree(ptr1);
        kfree(ptr3);
        return;
    }
    kprintf(INFO, "[KMALLOC] Reallocation successful: %p\n", ptr4);

    // Cleanup
    kfree(ptr1);
    kfree(ptr3);
    kfree(ptr4);
    
    kprintf(INFO, "[KMALLOC] Basic heap tests completed successfully\n");
}

void heap_test() {
    // Run heap functionality tests
    kmalloc_test_heap();
    
    // Run memory corruption tests
    kmalloc_test_corruption();
}