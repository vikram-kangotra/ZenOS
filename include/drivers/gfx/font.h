#pragma once

#include "multiboot2/multiboot_parser.h"

void set_scale(size_t _scale);
void draw_char(const struct multiboot_tag_framebuffer* fb_info, uint8_t c, uint32_t color);
