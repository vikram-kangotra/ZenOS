#include "kernel/kprintf.h"
#include "drivers/keyboard.h"
#include "string.h"
#include "stdbool.h"
#include "drivers/vga.h"
#include "kernel/mm/pmm.h"
#include "kernel/mm/vmm.h"
#include "arch/x86_64/interrupt/pit.h"
#include "multiboot2/multiboot2_parser.h"
#include "drivers/rtc.h"

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
    kprintf(CLI, "  help     - Show this help message\n");
    kprintf(CLI, "  clear    - Clear the screen\n");
    kprintf(CLI, "  echo     - Print arguments\n");
    kprintf(CLI, "  meminfo  - Show memory information\n");
    kprintf(CLI, "  sysinfo  - Show system information\n");
    kprintf(CLI, "  time     - Show current system time\n");
    kprintf(CLI, "  uptime   - Show system uptime\n");
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

static void cmd_meminfo(const char* args) {
    (void)args;
    uint64_t total_ram = get_total_ram();
    uint64_t used_ram = get_used_ram();
    uint64_t free_ram = total_ram - used_ram;
    
    kprintf(CLI, "Memory Information:\n");
    kprintf(CLI, "  Total RAM: %d KB\n", total_ram);
    kprintf(CLI, "  Used RAM:  %d KB\n", used_ram);
    kprintf(CLI, "  Free RAM:  %d KB\n", free_ram);
}

static void cmd_sysinfo(const char* args) {
    (void)args;
    kprintf(CLI, "System Information:\n");
    kprintf(CLI, "  Architecture: x86_64\n");
    kprintf(CLI, "  Kernel: ZenOS\n");
    kprintf(CLI, "  Memory: %d KB\n", get_total_ram());
}

static void cmd_time(const char* args) {
    (void)args;
    struct DateTime dt;
    rtc_get_time(&dt);
    
    // Add 2000 to year since RTC returns years since 2000
    kprintf(CLI, "Current time: %02d/%02d/%04d %02d:%02d:%02d\n", 
            dt.day, dt.month, dt.year + 2000, 
            dt.hours, dt.minutes, dt.seconds);
}

static void cmd_uptime(const char* args) {
    (void)args;
    uint32_t ticks = pit_get_ticks();
    uint32_t ms = pit_ticks_to_ms(ticks);
    uint32_t seconds = ms / 1000;
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    
    kprintf(CLI, "System uptime: ");
    if (days > 0) {
        kprintf(CLI, "%d day%s, ", days, days == 1 ? "" : "s");
    }
    kprintf(CLI, "%02d:%02d:%02d\n", 
            hours % 24, minutes % 60, seconds % 60);
}

// Command table
static const struct Command commands[] = {
    {"help", cmd_help, "Show help message"},
    {"clear", cmd_clear, "Clear the screen"},
    {"echo", cmd_echo, "Print arguments"},
    {"meminfo", cmd_meminfo, "Show memory information"},
    {"sysinfo", cmd_sysinfo, "Show system information"},
    {"time", cmd_time, "Show current system time"},
    {"uptime", cmd_uptime, "Show system uptime"},
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