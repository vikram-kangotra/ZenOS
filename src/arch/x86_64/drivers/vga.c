#include "drivers/vga.h"
#include "arch/x86_64/io.h"

#define FB_COMMAND_PORT     0x3D4
#define FB_DATA_PORT        0X3D5

#define FB_HIGH_BYTE_COMMAND    14
#define FB_LOW_BYTE_COMMAND     15

static const size_t NUM_COLS = 80;
static const size_t NUM_ROWS = 25;

struct Char {
    uint8_t character;
    uint8_t color;
};

struct Char volatile* buffer = (struct Char*) 0xB8000;

size_t col = 0;
size_t row = 0;
uint8_t color = PRINT_COLOR_WHITE | PRINT_COLOR_BLACK << 4;

void clear_row(size_t row) {
    struct Char empty = (struct Char) {
        .character = ' ',
        .color = color,
    };
    for (size_t col = 0; col < NUM_COLS; ++col) {
        buffer[col + NUM_COLS * row] = empty;
    }
}

void vga_clear_screen() {
    for (size_t i = 0; i < NUM_ROWS; ++i) {
        clear_row(i);
    }
}

void vga_set_color(uint8_t foreground, uint8_t background) {
    color = foreground + (background << 4);
}

void print_newline() {
    col = 0;

    if (row < NUM_ROWS - 1) {
        row++;
        return;
    }

    for (size_t row = 1; row < NUM_ROWS; row++) {
        for (size_t col = 0; col < NUM_COLS; col++) {
            struct Char character = buffer[col + NUM_COLS * row];
            buffer[col + NUM_COLS * (row - 1)] = character;
        }
    }

    clear_row(NUM_COLS - 1);
}

void vga_write_char(char character) {
    if (character == '\n') {
        print_newline();
        return;
    } 

    if (col > NUM_COLS - 1) {
        print_newline();
    }

    buffer[col + NUM_COLS * row] = (struct Char) {
        .character = character,
        .color = color,
    };
    

    col++;
}

void vga_set_cursor(int x, int y) {
    uint16_t position = y * NUM_COLS + x;
    out(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);
    out(FB_DATA_PORT, (position >> 8) & 0xFF); // High byte
    out(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
    out(FB_DATA_PORT, position & 0xFF);        // Low byte
}

