#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/kprintf.h"
#include "stdarg.h"

void kputchar(enum LogLevel level, char ch) {
    (void) level;
#ifndef DEBUG_MODE
    if (level != DEBUG) {
#endif
        vga_write_char(ch);
#ifndef DEBUG_MODE
    serial_write_char(ch);
}

void kprintf(enum LogLevel level, const char* format, ...) {
    // Get log configuration for this level
    const struct LogConfig* config = &log_configs[level];
    
    // Set colors
    vga_set_color(config->fg_color, config->bg_color);
    
    // Print prefix
    const char* prefix = config->prefix;
    while (*prefix) {
        kputchar(level, *prefix++);
    }

    // Parse and print format string
    va_list args;
    va_start(args, format);

    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++; // Skip '%'
            
            // Handle padding
            int padding = 0;
            if (*p == '0') {
                p++;
                while (*p >= '0' && *p <= '9') {
                    padding = padding * 10 + (*p - '0');
                    p++;
                }
            }

            // Handle String Max Length
            int str_max_length = -1;
            if (*p == '.') {
                p++;
                str_max_length = 0;
                while (*p >= '0' && *p <= '9') {
                    str_max_length = str_max_length * 10 + (*p - '0');
                    p++;
                }
            }
            
            switch (*p) {
                case 'c': {
                    char ch = va_arg(args, int);
                    kputchar(level, ch);
                    break;
                }
                case 's': {
                    const char* str = va_arg(args, const char*);
                    while (*str && str_max_length != 0) {
                        kputchar(level, *str++);
                        str_max_length--;
                    }
                    break;
                }
                case 'd': {
                    int value = va_arg(args, int);
                    if (value < 0) {
                        kputchar(level, '-');
                        value = -value;
                    }
                    char buffer[12];
                    int i = 0;
                    do {
                        buffer[i++] = '0' + value % 10;
                        value /= 10;
                    } while (value);
                    
                    // Apply padding
                    while (i < padding) {
                        kputchar(level, '0');
                        padding--;
                    }
                    
                    while (i--) {
                        kputchar(level, buffer[i]);
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
                    
                    // Apply padding
                    while (i < padding) {
                        kputchar(level, '0');
                        padding--;
                    }
                    
                    while (i--) {
                        kputchar(level, buffer[i]);
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
                        kputchar(level, buffer[i]);
                    }
                    break;
                }
                case 'p': {
                    uintptr_t ptr = va_arg(args, uintptr_t);
                    kputchar(level, '0'); kputchar(level, 'x');
                    char buffer[16];
                    int i = 0;
                    do {
                        int digit = ptr % 16;
                        buffer[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                        ptr /= 16;
                    } while (ptr);
                    while (i--) {
                        kputchar(level, buffer[i]);
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
                        kputchar(level, buffer[i]);
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
                        kputchar(level, buffer[i]);
                    }
                    break;
                }
                case '%': {
                    kputchar(level, '%');
                    break;
                }
                default:
                    kputchar(level, *p);
                    break;
            }
        } else {
            kputchar(level, *p);
        }
    }

    va_end(args);
    
    // Reset colors to default
    vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
}

