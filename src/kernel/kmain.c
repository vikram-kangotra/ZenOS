#include "drivers/vga.h"
#include "kernel/kprintf.h"

void kmain() {
    
    vga_enable_cursor(0, 15);

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    kprintf("Welcome to ZenOS");


}
