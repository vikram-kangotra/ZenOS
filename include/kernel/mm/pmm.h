#pragma once

#include "stdint.h"
#include "stddef.h"

#define PAGE_SIZE 4096

void buddy_init(uintptr_t mem_start, uint64_t mem_size);
void* buddy_alloc(size_t size);
void buddy_free(void* ptr);
