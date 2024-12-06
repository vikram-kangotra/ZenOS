#pragma once

#include "multiboot2.h"
#include "stdint.h"

typedef struct {
    void* address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
} framebuffer_info_t;

void parse_multiboot_tags(struct multiboot_tag* addr);

framebuffer_info_t* get_framebuffer_info();
