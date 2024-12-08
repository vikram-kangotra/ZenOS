#include "drivers/gfx.h"

uint32_t get_color(const struct multiboot_tag_framebuffer* fb_info, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (r << fb_info->framebuffer_red_field_position) |
        (g << fb_info->framebuffer_green_field_position) |
        (b << fb_info->framebuffer_blue_field_position) |
        (a << (fb_info->framebuffer_red_field_position + fb_info->framebuffer_green_field_position + fb_info->framebuffer_blue_field_position));
}

void put_pixel(const struct multiboot_tag_framebuffer* fb_info, size_t x, size_t y, uint32_t color) {
    uint32_t* fb_addr = (uint32_t*) fb_info->common.framebuffer_addr;
    uint32_t width = fb_info->common.framebuffer_width;
    fb_addr[y * width + x] = color;
}

void draw_line(const struct multiboot_tag_framebuffer* fb_info, size_t x1, size_t y1, size_t x2, size_t y2, uint32_t color) {
    for (size_t x = x1, y = y1; x <= x2 && y <= y2; ++x, ++y) {
        put_pixel(fb_info, x, y, color);
    }
}
