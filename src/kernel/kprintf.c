#include "drivers/vga.h"
#include "drivers/serial.h"

void kputchar(char ch) {
    vga_write_char(ch);
    serial_write_char(ch);
}

void kprintf(const char* format, ...) {
    uint32_t* args = (uint32_t*)&format + 1;

    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++; // Skip '%'
            switch (*p) {
                case 'c': {
                    char ch = (char)(*args++);
                    kputchar(ch);
                    break;
                }
                case 's': {
                    const char* str = (const char*)(*args++);
                    while (*str) {
                        kputchar(*str++);
                    }
                    break;
                }
                case 'd': {
                    int value = (int)(*args++);
                    if (value < 0) {
                        kputchar('-');
                        value = -value;
                    }
                    char buffer[10];
                    int i = 0;
                    do {
                        buffer[i++] = '0' + value % 10;
                        value /= 10;
                    } while (value);
                    while (i--) {
                        kputchar(buffer[i]);
                    }
                    break;
                }
                case '%': {
                    kputchar('%');
                    break;
                }
                default: {
                    kputchar('%');
                    kputchar(*p);
                    break;
                }
            }
        } else {
            kputchar(*p);
        }
    }
}
