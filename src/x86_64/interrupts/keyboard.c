#include <stdint.h>
#include "kernel/io.h"
#include "interrupts/pic.h"
#include "print.h"

char scancode_to_ascii(uint8_t scancode) {
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

void keyboard_handler() {
    outportb(PIC1_COMMAND, 0x20);

    uint8_t scan_code = inportb(0x60);

    kprintf("Scancode: %d\n", scan_code);

    char key = scancode_to_ascii(scan_code);

    if (key) {
        kprintf("ASCII: %c\n", key);
    }
}
