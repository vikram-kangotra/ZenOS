#include "arch/x86_64/interrupt/isr.h"
#include "arch/x86_64/interrupt/pic.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/io.h"
#include "stdint.h"

#define BUFFER_SIZE 256

static char key_buffer[BUFFER_SIZE];
static size_t buffer_read_index = 0;
static size_t buffer_write_index = 0;

static char scancode_to_ascii(uint8_t scancode) {
    static char ascii_table[] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',
        'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
    };
    if (scancode > 0 && scancode < sizeof(ascii_table))
        return ascii_table[scancode];
    return 0;
}

static void buffer_write(char key) {
    size_t next_index = (buffer_write_index + 1) % BUFFER_SIZE;
    if (next_index != buffer_read_index) {
        key_buffer[buffer_write_index] = key;
        buffer_write_index = next_index;
    }
}

char buffer_read() {
    if (buffer_read_index == buffer_write_index) {
        return 0;
    }
    char key = key_buffer[buffer_read_index];
    buffer_read_index = (buffer_read_index + 1) % BUFFER_SIZE;
    return key;
}

__attribute__((interrupt))
void irq_keyboard_handler(struct InterruptStackFrame* frame) {
    (void) frame;

    uint8_t scan_code = in(0x60);

    char key = scancode_to_ascii(scan_code);

    if (key) {
        buffer_write(key); // Store key in the buffer
    }

    pic_eoi(0x21);
}
