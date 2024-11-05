#include <stdbool.h>
#include <stdarg.h>
#include "print.h"

const static size_t NUM_COLS = 80;
const static size_t NUM_ROWS = 25;

struct Char {
    uint8_t character;
    uint8_t color;
};

struct Char volatile* buffer = (struct Char*) 0xb8000;

size_t col = 0;
size_t row = 0;
uint8_t color = PRINT_COLOR_WHITE | PRINT_COLOR_BLACK << 4;

void clear_row(size_t row) {
    struct Char empty = (struct Char) {
        character: ' ',
        color: color,
    };

    for (size_t col = 0; col < NUM_COLS; col++) {
        buffer[col + NUM_COLS * row] = empty;
    }
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

void print_clear() {
    for (size_t i = 0; i < NUM_ROWS; i++) {
        clear_row(i);
    }
}

void print_char(char character) {
    if (character == '\n') {
        print_newline();
        return;
    }

    if (col > NUM_COLS - 1) {
        print_newline();
    }

    buffer[col + NUM_COLS * row] = (struct Char) {
        character: (uint8_t) character,
        color: color
    };

    col++;
}

void print_str(const char* str) {
    for (size_t i = 0; 1; i++) {
        char character = (uint8_t) str[i];

        if (character == '\0') {
            return;
        }

        print_char(character);
    }
}

void print_set_color(uint8_t foreground, uint8_t background) {
    color = foreground + (background << 4);
}

#define to_hex(c) ((c) < 10 ? '0' + (c) : 'A' + ((c) - 10))

void print_hex(unsigned long num) {
    print_str("0x");

    if (num == 0) {
        print_char('0');
        return;
    }

    bool leading_zero = true;
    for (int i = (sizeof(num) * 8) - 4; i >= 0; i -= 4) {
        int nibble = (num >> i) & 0xF;

        if (nibble != 0 || !leading_zero) {
            print_char(to_hex(nibble));
            leading_zero = false;
        }
    }
}

void print_int(long num) {

    if (num == 0) {
        print_char('0');
        return;
    } 
    
    if (num<0) {
        print_char('-');
        num = -num;
    }

    char buff[20];
    size_t i = 0;
    
    while (num>0) {
        buff[i++] = (num % 10) + '0';
        num /= 10;
    }
    
    while (i>0) {
        print_char(buff[--i]);
    }
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    va_kprintf(format, args);

    va_end(args);
}

void va_kprintf (const char* format, va_list args) {

    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i+1] != '\0') {
            i++;
            switch (format[i]) {
                case 'd' : 
                    print_int(va_arg(args, int));
                    break;
                case 'x' :
                    print_hex(va_arg(args, unsigned int));
                    break;
                case 's' :
                    print_str(va_arg(args, const char*));
                    break;
                case 'c' :
                    print_char((char)va_arg(args, int));
                    break;
                case '%' :
                    print_char('%');
                    break;
                default : 
                    print_char('%');
                    print_char(format[i]);
            }
        } else {
            print_char(format[i]);
        }
    }
}
