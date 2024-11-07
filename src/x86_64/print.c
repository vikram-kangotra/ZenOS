#include <stdarg.h>
#include "print.h"

void error(const char* err, ...) {
    va_list args;

    va_start(args, err);
    va_kprintf(err, args);
    va_end(args);

    va_start(args, err);
    va_write_serial(err, args);
    va_end(args);
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    va_kprintf(format, args);

    va_end(args);
}

void write_serial(const char* format, ...) {
    va_list args;
    va_start(args, format);

    va_write_serial(format, args);

    va_end(args);
}
