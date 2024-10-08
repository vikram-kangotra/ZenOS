#include "print.h"
#include "kernel/serial.h"
#include "kernel/io.h"
#include "interrupts/idt.h"
#include "multiboot2/multiboot2.h"

void kernel_main(unsigned long magic, unsigned long addr) {

    init_serial();

    print_clear();

    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        kprintf("Invalid magic number: %x", magic);
        return;
    }

    if (addr & 7) {
        kprintf("unaligned mbi: %x", addr);
        return;
    }

    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    kprintf("Welcome to ZenOS Kernel\n");
    idt_init();
    sti();
}
