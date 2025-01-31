#include "multiboot2/multiboot2_parser.h"
#include "kernel/kprintf.h"

extern struct multiboot_tag* multiboot_addr;

static uint64_t total_ram = 0;

uint64_t get_total_ram() {
    return total_ram;
}

void multiboot2_parse() {
    
    for (struct multiboot_tag* tag = (struct multiboot_tag*)((multiboot_uint8_t*) multiboot_addr + 8);
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag*) ((multiboot_uint8_t*) tag + ((tag->size + 7) & ~7))) 
    {

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
                struct multiboot_tag_basic_meminfo* basic_meminfo = (struct multiboot_tag_basic_meminfo*) tag;
                total_ram = basic_meminfo->mem_upper - basic_meminfo->mem_lower;
            }
        }
    }
}
