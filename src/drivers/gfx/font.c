#include "drivers/gfx/font.h"
#include "drivers/gfx/gfx.h"
#include "stdint.h"

static size_t col = 0;
static size_t row = 0;
static const size_t CHAR_HEIGHT = 40;
static size_t scale = 1;
static size_t descender_offset = 10;

void clear_screen(const struct multiboot_tag_framebuffer* fb_info, uint32_t color) {
    size_t height = fb_info->common.framebuffer_height;
    size_t pitch = fb_info->common.framebuffer_pitch;
    uint32_t* framebuffer = (uint32_t*)fb_info->common.framebuffer_addr;

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < pitch / 4; x++) {
            framebuffer[y * (pitch / 4) + x] = color;
        }
    }

    col = 0;
    row = 0;
}

void set_scale(size_t _scale) {
    if (_scale > 0) {
        scale = _scale;
    }
}

static int is_descender(uint8_t c) {
    return c == 'g' || c == 'j' || c == 'p' || c == 'q' || c == 'y';
}

static void print_newline() {
    col = 0;
    row += CHAR_HEIGHT * scale;
}

void draw_char(const struct multiboot_tag_framebuffer* fb_info, uint8_t c, uint32_t color) {
    if (c == '\n') {
        print_newline();
        return;
    }

    if (c < 32 || c >= 128) {
        return;
    }

    size_t index = c - 1;
    const uint64_t* bitmap = font_bitmap[index];

    size_t current_character_width = bitmap[0];
    size_t current_character_height = bitmap[1];

    bitmap += 2;

    size_t vertical_offset = row + (CHAR_HEIGHT * scale) - (current_character_height * scale);
    if (is_descender(c)) {
        vertical_offset += descender_offset;
    }

    if (vertical_offset + (current_character_height * scale) > fb_info->common.framebuffer_height ||
        col + (current_character_width * scale) > fb_info->common.framebuffer_width) {
        return;
    }

    for (size_t y = 0; y < current_character_height; y++) {
        for (size_t x = 0; x < current_character_width; x++) {
            if ((bitmap[y] >> (current_character_width - 1 - x)) & 1) {
                for (size_t sy = 0; sy < scale; sy++) {
                    for (size_t sx = 0; sx < scale; sx++) {
                        put_pixel(fb_info, col + (x * scale), vertical_offset + (y * scale) + sy, color);
                    }
                }
            }
        }
    }

    col += current_character_width * scale; 
}
