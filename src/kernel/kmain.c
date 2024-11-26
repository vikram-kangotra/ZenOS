#include "drivers/vga.h"

void kmain() {
    
    vga_enable_cursor(0, 15);

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    const char* str = "Welcome to ZenOS";
    for (int i = 0; str[i] != '\0'; ++i) {
        vga_write_char(str[i]);
    }

}
