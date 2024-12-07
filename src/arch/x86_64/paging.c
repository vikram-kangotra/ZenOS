#include "arch/x86_64/asm.h"
#include "kernel/kprintf.h"

#define PAGE_PRESENT    0X01
#define PAGE_WRITABLE   0X02
#define PAGE_SIZE       4096

#define ALIGN_4K __attribute__((aligned(PAGE_SIZE)))

extern uint64_t ALIGN_4K pml4[512];

#define MAX_PAGE_TABLES 128

static uint64_t ALIGN_4K page_tables[MAX_PAGE_TABLES][PAGE_SIZE / sizeof(uint64_t)] = {0};
size_t next_free_page_table = 0;

uint64_t* allocate_page_table() {
    if (next_free_page_table >= MAX_PAGE_TABLES) {
        return 0;
    }

    uint64_t* table = page_tables[next_free_page_table];
    next_free_page_table++;
    return table;
}

void map_page(uint64_t virtual_address, uint64_t physical_address) {
    uint64_t pml4_index = (virtual_address >> 39) & 0x1FF;
    uint64_t pdpt_index = (virtual_address >> 30) & 0x1FF;
    uint64_t pd_index = (virtual_address >> 21) & 0x1FF;
    uint64_t pt_index = (virtual_address >> 12) & 0x1FF;

    if (!(pml4[pml4_index] & PAGE_PRESENT)) {
        pml4[pml4_index] = (uint64_t) allocate_page_table() | PAGE_PRESENT | PAGE_WRITABLE;
    }

    uint64_t* pdpt_entry = (uint64_t*) (pml4[pml4_index] & ~0xFFF);
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

void setup_paging() {

    for (size_t i = 0; i < 512; ++i) {
        map_page(i * PAGE_SIZE, i * PAGE_SIZE);
    }

    uint64_t framebuffer_address = 0xFD000000;
    size_t num_pages = 8294400 / PAGE_SIZE;
    for (size_t i = 0; i < num_pages; ++i) {
        map_page(framebuffer_address, framebuffer_address);
        framebuffer_address += PAGE_SIZE;
    }
}
