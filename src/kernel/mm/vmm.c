#include "arch/x86_64/asm.h"
#include "kernel/kprintf.h"

#define PAGE_PRESENT    0X01
#define PAGE_WRITABLE   0X02
#define PAGE_SIZE       4096

#define ALIGN_4K __attribute__((aligned(PAGE_SIZE)))

#define MAX_PAGE_TABLES 128

#define PAGE_ENTRIES 512

typedef ALIGN_4K struct PageTable {
    uintptr_t entries[PAGE_ENTRIES];
} PageTable;

extern PageTable pml4;

static PageTable page_tables[MAX_PAGE_TABLES];
size_t next_free_page_table = 0;

PageTable* allocate_page_table() {
    if (next_free_page_table >= MAX_PAGE_TABLES) {
        kprintf(ERROR, "Not enough space for Page Table");
        return 0;
    }

    PageTable* table = &page_tables[next_free_page_table];
    next_free_page_table++;
    return table;
}

uintptr_t virtual_to_physical(uintptr_t virtual_address) {
    uint64_t pml4_index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_address >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_address >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_address >> 12) & 0x1FF;

    if (!(pml4.entries[pml4_index] & PAGE_PRESENT)) {
        return 0; // Not mapped
    }

    uint64_t* pdpt_entry = (uint64_t*) (pml4.entries[pml4_index] & ~0xFFF);
    if (!(pdpt_entry[pdpt_index] & PAGE_PRESENT)) {
        return 0; // Not mapped
    }

    uint64_t* pd_entry = (uint64_t*) (pdpt_entry[pdpt_index] & ~0xFFF);
    if (!(pd_entry[pd_index] & PAGE_PRESENT)) {
        return 0; // Not mapped
    }

    uint64_t* pt_entry = (uint64_t*) (pd_entry[pd_index] & ~0xFFF);
    if (!(pt_entry[pt_index] & PAGE_PRESENT)) {
        return 0; // Not mapped
    }

    return pt_entry[pt_index] & ~0xFFF; // Return the physical address without flags
}

void map_virtual_to_physical(uintptr_t virtual_address, uintptr_t physical_address) {
    uint64_t pml4_index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_address >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_address >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_address >> 12) & 0x1FF;

    if (!(pml4.entries[pml4_index] & PAGE_PRESENT)) {
        pml4.entries[pml4_index] = (uint64_t) allocate_page_table() | PAGE_PRESENT | PAGE_WRITABLE;
    }

    uint64_t* pdpt_entry = (uint64_t*) (pml4.entries[pml4_index] & ~0xFFF);
    if (!(pdpt_entry[pdpt_index] & PAGE_PRESENT)) {
        pdpt_entry[pdpt_index] = (uint64_t) allocate_page_table() | PAGE_PRESENT | PAGE_WRITABLE;
    }

    uint64_t* pd_entry = (uint64_t*) (pdpt_entry[pdpt_index] & ~0xFFF);
    if (!(pd_entry[pd_index] & PAGE_PRESENT)) {
        pd_entry[pd_index] = (uint64_t) allocate_page_table() | PAGE_PRESENT | PAGE_WRITABLE;
    }

    uint64_t* pt_entry = (uint64_t*) (pd_entry[pd_index] & ~0xFFF);
    pt_entry[pt_index] = physical_address | PAGE_PRESENT | PAGE_WRITABLE;

    invlpg(virtual_address);
}
