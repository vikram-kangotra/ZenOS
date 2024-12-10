#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/gfx/gfx.h"
#include "drivers/gfx/font.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/interrupt/gdt.h"
#include "arch/x86_64/interrupt/idt.h"
#include "arch/x86_64/paging.h"
#include "multiboot2/multiboot_parser.h"
#include "kernel/shell.h"

void kmain(struct multiboot_tag* addr) {    

    init_serial();

    vga_enable_cursor(0, 15);

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    setup_paging();

    parse_multiboot_tags(addr);

    init_gdt_with_tss();
    init_idt();

    kprintf(INFO, "Welcome To ZenOS!\n");

    shell();
}
