#pragma once

#include "stdint.h"
#include "multiboot2/multiboot_parser.h"

#define GFX_COLOR_RED(fb_info) (get_color(fb_info, 0xff, 0x00, 0x00, 0xff))
#define GFX_COLOR_GREEN(fb_info) (get_color(fb_info, 0x00, 0xff, 0x00, 0xff))
#define GFX_COLOR_BLUE(fb_info) (get_color(fb_info, 0x00, 0x00, 0xff, 0xff))

uint32_t get_color(const struct multiboot_tag_framebuffer* fb_info, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void put_pixel(const struct multiboot_tag_framebuffer* fb_info, size_t x, size_t y, uint32_t color);

void draw_line(const struct multiboot_tag_framebuffer* fb_info, size_t x1, size_t y1, size_t x2, size_t y2, uint32_t color);
