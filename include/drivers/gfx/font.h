#pragma once

#include "multiboot2/multiboot_parser.h"

void draw_char(const struct multiboot_tag_framebuffer* fb_info, uint8_t c, size_t x, size_t y, uint32_t color);
