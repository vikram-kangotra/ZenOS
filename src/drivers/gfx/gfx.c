#include "drivers/gfx/gfx.h"
#include "stdint.h"
#include "math.h"
#include "string.h"

uint64_t foreground_color = 0xffffffff;
uint64_t background_color = 0xff000000;

void gfx_set_color(uint64_t foreground, uint64_t background) {
    foreground_color = foreground;
    background_color = background;
}

uint32_t get_color(const struct multiboot_tag_framebuffer* fb_info, 
    uint8_t r, uint8_t g, uint8_t b, uint8_t a
) {
    return (r << fb_info->framebuffer_red_field_position) |
        (g << fb_info->framebuffer_green_field_position) |
        (b << fb_info->framebuffer_blue_field_position) |
        (a << 24);
}

uint32_t alpha_blend(const struct multiboot_tag_framebuffer* fb_info, uint32_t src_color, uint32_t dst_color) {
    uint8_t src_a = (src_color >> 24) & 0xFF;
    uint8_t src_r = (src_color >> fb_info->framebuffer_red_field_position) & 0xFF;
    uint8_t src_g = (src_color >> fb_info->framebuffer_green_field_position) & 0xFF;
    uint8_t src_b = (src_color >> fb_info->framebuffer_blue_field_position) & 0xFF;

    uint8_t dst_a = (dst_color >> 24) & 0xFF;
    uint8_t dst_r = (dst_color >> fb_info->framebuffer_red_field_position) & 0xFF;
    uint8_t dst_g = (dst_color >> fb_info->framebuffer_green_field_position) & 0xFF;
    uint8_t dst_b = (dst_color >> fb_info->framebuffer_blue_field_position) & 0xFF;

    if (src_a == 0) {
        return dst_color;
    }

    uint8_t out_a = src_a + dst_a * (255 - src_a) / 255;

    uint8_t out_r = (src_r * src_a + dst_r * dst_a * (255 - src_a) / 255) / (out_a ? out_a : 1);
    uint8_t out_g = (src_g * src_a + dst_g * dst_a * (255 - src_a) / 255) / (out_a ? out_a : 1);
    uint8_t out_b = (src_b * src_a + dst_b * dst_a * (255 - src_a) / 255) / (out_a ? out_a : 1);

    return get_color(fb_info, out_r, out_g, out_b, out_a);
}

uint32_t get_pixel(const struct multiboot_tag_framebuffer* fb_info, size_t x, size_t y) {
    uint32_t* fb_addr = (uint32_t*) fb_info->common.framebuffer_addr;
    uint32_t width = fb_info->common.framebuffer_width;
    return fb_addr[y * width + x];
}

void put_pixel(const struct multiboot_tag_framebuffer* fb_info, size_t x, size_t y, uint32_t color) {

    color = foreground_color;

    uint32_t* fb_addr = (uint32_t*) fb_info->common.framebuffer_addr;
    uint32_t width = fb_info->common.framebuffer_width;
    uint32_t src_color = fb_addr[y * width + x];
    fb_addr[y * width + x] = alpha_blend(fb_info, color, src_color);
}

void draw_line(
    const struct multiboot_tag_framebuffer* fb_info, 
    size_t x1, size_t y1, size_t x2, size_t y2, 
    uint32_t color
) {
    int dx = abs((int)x2 - (int)x1);
    int dy = abs((int)y2 - (int)y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;

    int err = dx - dy;

    while (1) {
        put_pixel(fb_info, x1, y1, color);

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}


void draw_rectangle(
    const struct multiboot_tag_framebuffer* fb_info,
    size_t x1, size_t y1, size_t x2, size_t y2,
    uint32_t color
) {
    for (size_t x = x1; x <= x2; ++x) {
        put_pixel(fb_info, x, y1, color);
    }
    for (size_t x = x1; x <= x2; ++x) {
        put_pixel(fb_info, x, y2, color);
    }
    for (size_t y = y1; y <= y2; ++y) {
        put_pixel(fb_info, x1, y, color);
    }
    for (size_t y = y1; y <= y2; ++y) {
        put_pixel(fb_info, x2, y, color);
    }
}

void fill_rectangle(
    const struct multiboot_tag_framebuffer* fb_info,
    size_t x1, size_t y1, size_t x2, size_t y2,
    uint32_t color
) {
    size_t pitch = fb_info->common.framebuffer_pitch;
    size_t bpp = fb_info->common.framebuffer_bpp / 8; // Bytes per pixel
    uint8_t* framebuffer = (uint8_t*)fb_info->common.framebuffer_addr;

    for (size_t y = y1; y <= y2; ++y) {
        uint8_t* row = framebuffer + (y * pitch) + (x1 * bpp);
        for (size_t x = x1; x <= x2; ++x) {
            uint32_t dst_color = *((uint32_t*)row);
            *((uint32_t*)row) = alpha_blend(fb_info, color, dst_color);
            row += bpp;
        }
    }
}

void draw_circle(
    const struct multiboot_tag_framebuffer* fb_info, 
    size_t center_x, size_t center_y, size_t radius, 
    uint32_t color
) {
    int x = radius;
    int y = 0;
    int decision_over2 = 1 - x;

    while (y <= x) {
        put_pixel(fb_info, center_x + x, center_y + y, color);
        put_pixel(fb_info, center_x + y, center_y + x, color);
        put_pixel(fb_info, center_x - x, center_y + y, color);
        put_pixel(fb_info, center_x - y, center_y + x, color);
        put_pixel(fb_info, center_x + x, center_y - y, color);
        put_pixel(fb_info, center_x + y, center_y - x, color);
        put_pixel(fb_info, center_x - x, center_y - y, color);
        put_pixel(fb_info, center_x - y, center_y - x, color);
        
        y++;
        if (decision_over2 <= 0) {
            decision_over2 += (2 * y + 1);
        } else {
            x--;
            decision_over2 += (2 * y + 1 - 2 * x);
        }
    }
}

void fill_circle(const struct multiboot_tag_framebuffer* fb_info, size_t xc, size_t yc, size_t radius, uint32_t color) {
    int x = 0;
    int y = radius;
    int d = 1 - radius;

    while (x <= y) {
        draw_line(fb_info, xc - x, yc + y, xc + x, yc + y, color);
        draw_line(fb_info, xc - x, yc - y, xc + x, yc - y, color);
        draw_line(fb_info, xc - y, yc + x, xc + y, yc + x, color);
        draw_line(fb_info, xc - y, yc - x, xc + y, yc - x, color);

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}
