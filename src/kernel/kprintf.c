#include "drivers/vga.h"
#include "drivers/serial.h"

void kputchar(char ch) {
    vga_write_char(ch);
    serial_write_char(ch);
}

#define GET_ARG(type, reg_index, stack_pointer)                \
    ((reg_index < 5) ? ((type)(reg_args[reg_index++])) :       \
                       ((type)(*stack_pointer++)))

void kprintf(const char* format, ...) {
    uintptr_t reg_args[5];
    asm volatile (
        "mov %%rsi, %0\n\t"
        "mov %%rdx, %1\n\t"
        "mov %%rcx, %2\n\t"
        "mov %%r8,  %3\n\t"
        "mov %%r9,  %4\n\t"
        : "=m"(reg_args[0]), "=m"(reg_args[1]), "=m"(reg_args[2]),
          "=m"(reg_args[3]), "=m"(reg_args[4])
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
