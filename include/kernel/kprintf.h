#pragma once

enum LogLevel {
    INFO,
    DEBUG,
    WARN,
    ERROR,
};

void kprintf(enum LogLevel, const char* format, ...);
