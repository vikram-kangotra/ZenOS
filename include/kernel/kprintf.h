#pragma once

#include "drivers/vga.h"

// Log levels with their prefixes and colors
enum LogLevel {
    DEBUG,      // Debug information
    INFO,       // General information
    WARN,       // Warning messages
    ERROR,      // Error messages
    FATAL,      // Fatal errors
    SUCCESS,    // Success messages
    CLI         // CLI output (no prefix)
};

// Log level configuration
struct LogConfig {
    const char* prefix;    // Prefix for the log message
    uint8_t fg_color;      // Foreground color
    uint8_t bg_color;      // Background color
};

// Default log configurations
static const struct LogConfig log_configs[] = {
    [DEBUG]   = { "[DEBUG] ", PRINT_COLOR_DARK_GRAY, PRINT_COLOR_BLACK },
    [INFO]    = { "[INFO]  ", PRINT_COLOR_WHITE, PRINT_COLOR_BLACK },
    [WARN]    = { "[WARN]  ", PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK },
    [ERROR]   = { "[ERROR] ", PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK },
    [FATAL]   = { "[FATAL] ", PRINT_COLOR_RED, PRINT_COLOR_BLACK },
    [SUCCESS] = { "[OK]    ", PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK },
    [CLI]     = { "", PRINT_COLOR_WHITE, PRINT_COLOR_BLACK }
};

/**
 * @brief Print a formatted string with log level
 * 
 * @param level Log level (DEBUG, INFO, WARN, ERROR, FATAL, SUCCESS)
 * @param format Format string
 * @param ... Variable arguments
 */
void kprintf(enum LogLevel level, const char* format, ...);
