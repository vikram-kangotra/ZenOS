#pragma once

#include "stdint.h"
#include "stddef.h"

void buddy_init(uintptr_t mem_start, uint64_t mem_size);
void* buddy_alloc(size_t size);
void buddy_free(void* ptr);
uint64_t get_used_ram(void);

// Memory statistics
uint64_t get_free_ram(void);
uint64_t get_fragmented_ram(void);

// Testing
void test_buddy_allocator(void);