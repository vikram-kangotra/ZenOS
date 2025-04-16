#include "kernel/kprintf.h"
#include "drivers/keyboard.h"
#include "string.h"
#include "stdbool.h"
#include "drivers/vga.h"

#define CLI_BUFFER_SIZE 256
#define CLI_PROMPT "ZenOS> "

// Command structure
struct Command {
    const char* name;
    void (*handler)(const char* args);
    const char* description;
};

// Command handlers
static void cmd_help(const char* args) {
    (void)args;
    kprintf(CLI, "Available commands:\n");
    kprintf(CLI, "  help    - Show this help message\n");
    kprintf(CLI, "  clear   - Clear the screen\n");
    kprintf(CLI, "  echo    - Print arguments\n");
}

static void cmd_clear(const char* args) {
    (void)args;
    vga_clear_screen();
    kprintf(CLI, CLI_PROMPT);
}

static void cmd_echo(const char* args) {
    if (args && *args) {
        kprintf(CLI, "%s\n", args);
    } else {
        kprintf(ERROR, "Usage: echo <text>\n");
    }
}

// Command table
static const struct Command commands[] = {
    {"help", cmd_help, "Show help message"},
    {"clear", cmd_clear, "Clear the screen"},
    {"echo", cmd_echo, "Print arguments"},
    {NULL, NULL, NULL}  // End marker
};

// CLI state
static char _input_buffer[CLI_BUFFER_SIZE];
static size_t _buffer_pos = 0;

// Process a command line
static void process_command(const char* line) {
    if (!line || !*line) return;

    // Find command and arguments
    const char* cmd_end = strchr(line, ' ');
    const char* cmd = line;
    const char* args = NULL;
    
    if (cmd_end) {
        args = cmd_end + 1;
        while (*args == ' ') args++;  // Skip leading spaces
    }

    // Find and execute command
    for (const struct Command* cmd_ptr = commands; cmd_ptr->name; cmd_ptr++) {
        if (strncmp(cmd, cmd_ptr->name, cmd_end ? (size_t) (cmd_end - cmd) : strlen(cmd)) == 0) {
            cmd_ptr->handler(args);
            return;
        }
    }

    // Print error message using supported format specifiers
    kprintf(ERROR, "Unknown command: ");
    if (cmd_end) {
        char cmd_name[256];
        size_t len = cmd_end - cmd;
        strncpy(cmd_name, cmd, len);
        cmd_name[len] = '\0';
        kprintf(ERROR, "%s\n", cmd_name);
    } else {
        kprintf(ERROR, "%s\n", cmd);
    }
}

// Handle special keys
static bool handle_special_keys(char c) {
    switch (c) {
        case '\b':  // Backspace
            if (_buffer_pos > 0) {
                _buffer_pos--;
                kprintf(CLI, "\b \b");  // Erase character
            }
            return true;
        case '\n':  // Enter
            kprintf(CLI, "\n");
            _input_buffer[_buffer_pos] = '\0';
            process_command(_input_buffer);
            _buffer_pos = 0;
            kprintf(CLI, CLI_PROMPT);
            return true;
        default:
            return false;
    }
}

// Main CLI loop
void cli_run(void) {
    kprintf(CLI, "\nWelcome to ZenOS CLI\n");
    kprintf(CLI, CLI_PROMPT);

    while (true) {
        char c = keyboard_read_blocking();
        
        // Handle special keys
        if (handle_special_keys(c)) {
            continue;
        }

        // Handle regular input
        if (_buffer_pos < CLI_BUFFER_SIZE - 1) {
            _input_buffer[_buffer_pos++] = c;
            kprintf(CLI, "%c", c);
        }
    }
} 