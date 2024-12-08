#pragma once

#include "multiboot2.h"
#include "stdint.h"

void parse_multiboot_tags(struct multiboot_tag* addr);

struct multiboot_tag_framebuffer* get_framebuffer_info();
