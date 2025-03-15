#include "kernel/kernel.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/interrupt/gdt.h"
#include "arch/x86_64/interrupt/idt.h"
#include "multiboot2/multiboot2_parser.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "arch/x86_64/interrupt/pit.h"

void kmain() {

    multiboot2_parse();

    init_serial();

    vga_enable_cursor(0, 15);

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    init_gdt_with_tss();

    init_pit(100);
    init_idt();

    buddy_init((uintptr_t) &KERNEL_END, get_total_ram() << 10);

    kprintf(INFO, "Welcome to ZenOS\n");
}
