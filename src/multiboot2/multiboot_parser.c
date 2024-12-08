#include "multiboot2/multiboot2.h"
#include "multiboot2/multiboot_parser.h"
#include "kernel/kprintf.h"

struct multiboot_tag_framebuffer framebuffer_info;

struct multiboot_tag_framebuffer* get_framebuffer_info() {
    return &framebuffer_info;
}

void parse_framebuffer_tag(struct multiboot_tag_framebuffer* framebuffer_tag) {
    framebuffer_info = *framebuffer_tag;
}

void parse_multiboot_tags(struct multiboot_tag* addr) {

    for (struct multiboot_tag* tag = (struct multiboot_tag*)((multiboot_uint8_t*) addr + 8);
            tag->type != MULTIBOOT_TAG_TYPE_END;
            tag = (struct multiboot_tag*) ((multiboot_uint8_t*) tag + ((tag->size + 7) & ~7))) 
    {

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: parse_framebuffer_tag((struct multiboot_tag_framebuffer*) tag); break;
        }
    }

}
