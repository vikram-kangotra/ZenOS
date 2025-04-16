#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/kprintf.h"
#include "stdarg.h"

void kputchar(char ch) {
    vga_write_char(ch);
    serial_write_char(ch);
}

void kprintf(enum LogLevel level, const char* format, ...) {
    switch (level) {
        case INFO: vga_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK); break;
        case DEBUG: vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK); break;
        case WARN: vga_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK); break;
        case ERROR: vga_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK); break;
    }

    va_list args;
    va_start(args, format);

    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++; // Skip '%'
            switch (*p) {
                case 'c': {
                    char ch = va_arg(args, int);
                    kputchar(ch);
                    break;
                }
                case 's': {
                    const char* str = va_arg(args, const char*);
                    while (*str) {
                        kputchar(*str++);
                    }
                    break;
                }
                case 'd': {
                    int value = va_arg(args, int);
                    if (value < 0) {
                        kputchar('-');
                        value = -value;
                    }
                    char buffer[12];
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
                case 'u': {
                    unsigned int value = va_arg(args, unsigned int);
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
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    char buffer[8];
                    int i = 0;
                    do {
                        int digit = value % 16;
                        buffer[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                        value /= 16;
                    } while (value);
                    while (i--) {
                        kputchar(buffer[i]);
                    }
                    break;
                }
                case 'p': {
                    uintptr_t ptr = va_arg(args, uintptr_t);
                    kputchar('0'); kputchar('x');
                    char buffer[16];
                    int i = 0;
                    do {
                        int digit = ptr % 16;
                        buffer[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                        ptr /= 16;
                    } while (ptr);
                    while (i--) {
                        kputchar(buffer[i]);
                    }
                    break;
                }
                case 'o': {
                    unsigned int value = va_arg(args, unsigned int);
                    char buffer[11];
                    int i = 0;
                    do {
                        buffer[i++] = '0' + (value % 8);
                        value /= 8;
                    } while (value);
                    while (i--) {
                        kputchar(buffer[i]);
                    }
                    break;
                }
                case 'b': {
                    unsigned int value = va_arg(args, unsigned int);
                    char buffer[32];
                    int i = 0;
                    do {
                        buffer[i++] = '0' + (value & 1);
                        value >>= 1;
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
                default:
                    kputchar(*p);
                    break;
            }
        } else {
            kputchar(*p);
        }
    }

    va_end(args);
}

