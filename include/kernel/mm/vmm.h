#pragma once

#include "kernel/mm/pmm.h"

// Page protection flags
#define PAGE_PRESENT    0x01
#define PAGE_WRITABLE   0x02
#define PAGE_USER       0x04
#define PAGE_NOCACHE    0x08
#define PAGE_WASM       0x10  // Special flag for WASM pages

void map_virtual_to_physical(uintptr_t virtual_address, uintptr_t physical_address, uint8_t flags);

uintptr_t virtual_to_physical(uintptr_t virtual_address);
