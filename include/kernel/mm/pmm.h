#pragma once

void buddy_init(uintptr_t mem_start, uint64_t mem_size);
void* buddy_alloc(size_t size);
void buddy_free(void* ptr);
