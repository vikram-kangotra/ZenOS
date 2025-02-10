#include "kernel/kernel.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/interrupt/gdt.h"
#include "arch/x86_64/interrupt/idt.h"
#include "multiboot2/multiboot2_parser.h"
#include "kernel/mm/pmm.h"
#include "kernel/mem.h"

void kmain() {

    multiboot2_parse();

    init_serial();

    vga_enable_cursor(0, 15);

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    init_gdt_with_tss();
    init_idt();

    kprintf(INFO, "Welcome to ZenOS\n");
    
    kprintf(DEBUG, "KERNEL_END: %p\n", &KERNEL_END);
    kprintf(DEBUG, "Total Memory: %d\n", get_total_ram());

    buddy_init((uintptr_t) &KERNEL_END, get_total_ram());

    void* ptr1 = buddy_alloc(4096);
    buddy_free(ptr1);
    void* ptr2 = buddy_alloc(4096);
    buddy_free(ptr2);
    void* ptr3 = buddy_alloc(4096);
    buddy_free(ptr3);

    kprintf(INFO, "PTR1: %p\n", ptr1);
    kprintf(INFO, "PTR2: %p\n", ptr2);
    kprintf(INFO, "PTR3: %p\n", ptr3);
}
