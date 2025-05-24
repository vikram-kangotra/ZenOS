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
#include "kernel/mm/kmalloc.h"
#include "kernel/cli/cli.h"
#include "drivers/rtc.h"
#include "fs/vfs.h"
#include "drivers/ata.h"
#include "string.h"
#include "wasm/wasm_kernel.h"

void kmain() {
    multiboot2_parse();
    init_serial();
    vga_enable_cursor(0, 15);
    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    // Initialize GDT first (required for protected mode)
    init_gdt_with_tss();
    
    // Initialize IDT and PIC
    init_idt();  // This also enables interrupts with sti()
    
    // Initialize PIT after IDT is set up
    init_pit(100);

    // Initialize RTC
    rtc_init();

    // Initialize memory management
    buddy_init((uintptr_t) &KERNEL_END, get_total_ram());
    test_buddy_allocator();  // Run buddy allocator tests

    kmalloc_init();
    heap_test();
    
    // Initialize filesystem
    vfs_init();

    // Run ATA tests
    kprintf(INFO, "\nRunning ATA driver tests...\n");
    run_ata_tests();
    
    kprintf(INFO, "Welcome to ZenOS\n");

    // Initialize WebAssembly runtime
    wasm_test();

    // Start the command line interface
    cli_run();

    // Main kernel loop
    while (1) {
        // Kernel idle loop
        asm("hlt");
    }
}

