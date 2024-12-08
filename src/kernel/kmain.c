#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/gfx.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/interrupt/gdt.h"
#include "arch/x86_64/interrupt/idt.h"
#include "arch/x86_64/paging.h"
#include "multiboot2/multiboot_parser.h"

void kmain(struct multiboot_tag* addr) {    

    init_serial();

    vga_enable_cursor(0, 15);

    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    vga_clear_screen();

    setup_paging();

    init_gdt_with_tss();
    init_idt();

    parse_multiboot_tags(addr);

    struct multiboot_tag_framebuffer* fb_info = get_framebuffer_info();
    draw_line(fb_info, 100, 100, 300, 300, GFX_COLOR_BLUE(fb_info));
}
