#include "drivers/vga.h"
#include "drivers/serial.h"
#include "kernel/kprintf.h"
#include "drivers/gfx/gfx.h"
#include "drivers/gfx/font.h"

void kputchar(char ch) {
    vga_write_char(ch);
    serial_write_char(ch);

    struct multiboot_tag_framebuffer* fb_info = get_framebuffer_info();
    draw_char(fb_info, ch, 0xffffffff);
}

#define GET_ARG(type, reg_index, stack_pointer)                \
    ((reg_index < 4) ? ((type)(reg_args[reg_index++])) :       \
                       ((type)(*stack_pointer++)))

void kprintf(enum LogLevel level, const char* format, ...) {

    switch (level) {
        case INFO: {
            vga_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK); 
            gfx_set_color(0xff00ff00, 0xff000000);
        } break;
        case DEBUG: {
            vga_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            gfx_set_color(0xffffffff, 0xff000000);
        } break;
        case WARN: vga_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK); break;
        case ERROR: {
            vga_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK); 
            gfx_set_color(0xffff0000, 0xff000000);
        } break;
    }

    uintptr_t reg_args[4];
    asm volatile (
        "mov %%rdx, %0\n\t"
        "mov %%rcx, %1\n\t"
        "mov %%r8,  %2\n\t"
        "mov %%r9,  %3\n\t"
        : "=m"(reg_args[0]), "=m"(reg_args[1]), 
          "=m"(reg_args[2]), "=m"(reg_args[3])
    );

    uintptr_t* stack_args = (uintptr_t*)__builtin_frame_address(0) + 2;
    int reg_index = 0;

    for (const char* p = format; *p != '\0'; p++) {
        if (*p == '%' && *(p + 1) != '\0') {
            p++; // Skip '%'
            switch (*p) {
                case 'c': {
                    char ch = GET_ARG(char, reg_index, stack_args);
                    kputchar(ch);
                    break;
                }
                case 's': {
                    const char* str = GET_ARG(const char*, reg_index, stack_args);
                    while (*str) {
                        kputchar(*str++);
                    }
                    break;
                }
                case 'd': {
                    int value = GET_ARG(int, reg_index, stack_args);
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
                    unsigned int value = GET_ARG(unsigned int, reg_index, stack_args);
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
                    unsigned int value = GET_ARG(unsigned int, reg_index, stack_args);
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
                    uintptr_t ptr = GET_ARG(uintptr_t, reg_index, stack_args);
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
                    unsigned int value = GET_ARG(unsigned int, reg_index, stack_args);
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
                    unsigned int value = GET_ARG(unsigned int, reg_index, stack_args);
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

