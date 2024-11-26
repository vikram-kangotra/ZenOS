#include "drivers/vga.h"

void kmain() {

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

}
