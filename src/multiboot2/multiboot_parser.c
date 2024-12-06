#include "multiboot2/multiboot2.h"
#include "multiboot2/multiboot_parser.h"
#include "kernel/kprintf.h"

static framebuffer_info_t framebuffer_info;

framebuffer_info_t* get_framebuffer_info() {
    return &framebuffer_info;
}

void parse_framebuffer_tag(struct multiboot_tag_framebuffer* framebuffer_tag) {

    framebuffer_info.address = (void*) framebuffer_tag->common.framebuffer_addr;
    framebuffer_info.bpp = framebuffer_tag->common.framebuffer_bpp;
    framebuffer_info.height = framebuffer_tag->common.framebuffer_height;
    framebuffer_info.width = framebuffer_tag->common.framebuffer_width;
    framebuffer_info.pitch = framebuffer_tag->common.framebuffer_pitch;
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
