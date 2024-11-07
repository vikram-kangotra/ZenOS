#pragma once

#include "kernel/serial.h"
#include "kernel/vga.h"

void error(const char* err, ...);
void kprintf(const char* format, ...);
void write_serial(const char* format, ...);
