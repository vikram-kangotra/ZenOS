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
#include "kernel/fs/vfs.h"

// Test different allocation sizes
#define SMALL_SIZE 16
#define MEDIUM_SIZE 1024
#define LARGE_SIZE 4096
#define HUGE_SIZE (1024 * 1024)  // 1MB

void heap_test() {
    kprintf(INFO, "Starting heap tests...\n");

    // Test 1: Basic allocations
    void* ptr1 = kmalloc(SMALL_SIZE);
    void* ptr2 = kmalloc(MEDIUM_SIZE);
    void* ptr3 = kmalloc(LARGE_SIZE);
    
    kprintf(INFO, "Basic allocations:\n");
    kprintf(INFO, "Small (%d bytes): %p\n", SMALL_SIZE, ptr1);
    kprintf(INFO, "Medium (%d bytes): %p\n", MEDIUM_SIZE, ptr2);
    kprintf(INFO, "Large (%d bytes): %p\n", LARGE_SIZE, ptr3);

    // Test 2: Alignment check
    if ((uintptr_t)ptr1 % 16 == 0 && 
        (uintptr_t)ptr2 % 16 == 0 && 
        (uintptr_t)ptr3 % 16 == 0) {
        kprintf(INFO, "Alignment test passed\n");
    } else {
        kprintf(ERROR, "Alignment test failed\n");
    }

    // Test 3: Free and reallocate
    kfree(ptr2);
    void* ptr4 = kmalloc(MEDIUM_SIZE);
    kprintf(INFO, "Reallocation after free: %p\n", ptr4);

    // Test 4: Multiple small allocations
    void* small_ptrs[10];
    kprintf(INFO, "Multiple small allocations:\n");
    for (int i = 0; i < 10; i++) {
        small_ptrs[i] = kmalloc(SMALL_SIZE);
        kprintf(INFO, "Allocation %d: %p\n", i, small_ptrs[i]);
    }

    // Test 5: Free all small allocations
    for (int i = 0; i < 10; i++) {
        kfree(small_ptrs[i]);
    }

    // Test 6: Large allocation
    void* huge_ptr = kmalloc(HUGE_SIZE);
    if (huge_ptr) {
        kprintf(INFO, "Huge allocation (%d bytes) successful: %p\n", HUGE_SIZE, huge_ptr);
        kfree(huge_ptr);
    } else {
        kprintf(ERROR, "Huge allocation failed\n");
    }

    // Test 7: Memory reuse test
    void* test_ptr = kmalloc(MEDIUM_SIZE);
    kprintf(INFO, "Allocated test block: %p\n", test_ptr);
    kfree(test_ptr);
    void* reused_ptr = kmalloc(MEDIUM_SIZE);
    kprintf(INFO, "Reused memory block: %p\n", reused_ptr);
    kfree(reused_ptr);

    // Test 8: Zero size allocation
    void* zero_ptr = kmalloc(0);
    if (zero_ptr == NULL) {
        kprintf(INFO, "Zero size allocation test passed\n");
    } else {
        kprintf(ERROR, "Zero size allocation test failed\n");
    }

    // Test 9: Free NULL pointer
    kfree(NULL);
    kprintf(INFO, "NULL pointer free test passed\n");

    // Cleanup remaining allocations
    kfree(ptr1);
    kfree(ptr3);
    kfree(ptr4);

    kprintf(INFO, "Heap tests completed\n");
}

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
    buddy_init((uintptr_t) &KERNEL_END, get_total_ram() << 10);
    
    // Initialize filesystem
    vfs_init();
    
    kprintf(INFO, "Welcome to ZenOS\n");

    // Start the command line interface
    cli_run();
}
