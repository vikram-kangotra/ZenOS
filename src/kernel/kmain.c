#include "kernel/kernel.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/gfx/gfx.h"
#include "drivers/gfx/font.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/interrupt/gdt.h"
#include "arch/x86_64/interrupt/idt.h"
<<<<<<< HEAD
#include "arch/x86_64/paging.h"
#include "multiboot2/multiboot_parser.h"
#include "kernel/shell.h"

void kmain(struct multiboot_tag* addr) {    
=======
#include "multiboot2/multiboot2_parser.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"

void kmain() {

    multiboot2_parse();
>>>>>>> refs/remotes/origin/main

    init_serial();

    vga_enable_cursor(0, 15);

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    setup_paging();

    parse_multiboot_tags(addr);

    init_gdt_with_tss();
    init_idt();

<<<<<<< HEAD
    kprintf(INFO, "Welcome To ZenOS!\n");

    shell();
=======
    kprintf(INFO, "Welcome to ZenOS\n");
    
    kprintf(DEBUG, "KERNEL_END: %p\n", &KERNEL_END);
    kprintf(DEBUG, "Total Memory: %d\n", get_total_ram());

    buddy_init((uintptr_t) &KERNEL_END, get_total_ram() << 10);

    int* fb = (int*) 0x00008fffffff;
    *fb = 1234;

    kprintf(DEBUG, "fb = %d\n", *fb);

    kprintf(DEBUG, "Physical Address = %p", virtual_to_physical((uintptr_t)fb));

>>>>>>> refs/remotes/origin/main
}
