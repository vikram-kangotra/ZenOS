#pragma once

#include <stdarg.h>

int init_serial();
void write_serial(const char* format, ...);
void va_write_serial(const char* format, va_list args);
void write_serial_char(char a);
void write_serial_string(const char* a);
void write_serial_hex(unsigned long hex);
