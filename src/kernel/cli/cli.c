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
#include "kernel/fs/vfs.h"

#define CLI_BUFFER_SIZE 256

// Command structure
struct Command {
    const char* name;
    void (*handler)(const char* args);
    const char* description;
};

static void cmd_help(const char* args);
static void cmd_clear(const char* args);
static void cmd_echo(const char* args);
static void cmd_meminfo(const char* args);
static void cmd_sysinfo(const char* args);
static void cmd_time(const char* args);
static void cmd_uptime(const char* args);
static void cmd_ls(const char* args);
static void cmd_cd(const char* args);
static void cmd_mkdir(const char* args);
static void cmd_touch(const char* args);
static void cmd_cat(const char* args);

// Command table
static const struct Command commands[] = {
    {"help", cmd_help, "Show help message"},
    {"clear", cmd_clear, "Clear the screen"},
    {"echo", cmd_echo, "Print arguments"},
    {"meminfo", cmd_meminfo, "Show memory information"},
    {"sysinfo", cmd_sysinfo, "Show system information"},
    {"time", cmd_time, "Show current system time"},
    {"uptime", cmd_uptime, "Show system uptime"},
    {"ls", cmd_ls, "List directory contents"},
    {"cd", cmd_cd, "Change directory"},
    {"mkdir", cmd_mkdir, "Create directory"},
    {"touch", cmd_touch, "Create empty file"},
    {"cat", cmd_cat, "Display file contents"},
    {NULL, NULL, NULL}  // End marker
};

// Command handlers
static void cmd_help(const char* args) {
    (void)args;
    kprintf(CLI, "Available commands:\n");
    for (const struct Command* cmd = commands; cmd->name; cmd++) {
        kprintf(CLI, "  %s - %s\n", cmd->name, cmd->description);
    }
}

static void cmd_clear(const char* args) {
    (void)args;
    vga_clear_screen();
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

static void cmd_ls(const char* args) {
    (void)args;
    struct vfs_node* dir = vfs_getcwd();
    if (!dir) {
        kprintf(ERROR, "Failed to get current directory\n");
        return;
    }
    
    uint32_t index = 0;
    struct vfs_node* entry;
    while ((entry = vfs_readdir(dir, index++)) != NULL) {
        const char* type = (entry->flags & FS_DIRECTORY) ? "d" : "-";
        kprintf(CLI, "%s %s\n", type, entry->name);
    }
}

static void cmd_cd(const char* args) {
    if (!args || !*args) {
        kprintf(ERROR, "Usage: cd <directory>\n");
        return;
    }
    
    if (!vfs_chdir(args)) {
        kprintf(ERROR, "Failed to change directory\n");
    }
}

static void cmd_mkdir(const char* args) {
    if (!args || !*args) {
        kprintf(ERROR, "Usage: mkdir <directory>\n");
        return;
    }
    
    struct vfs_node* dir = vfs_create_node(args, FS_DIRECTORY);
    if (!dir) {
        kprintf(ERROR, "Failed to create directory\n");
        return;
    }
}

static void cmd_touch(const char* args) {
    if (!args || !*args) {
        kprintf(ERROR, "Usage: touch <file>\n");
        return;
    }
    
    struct vfs_node* file = vfs_create_node(args, FS_FILE);
    if (!file) {
        kprintf(ERROR, "Failed to create file\n");
        return;
    }
}

static void cmd_cat(const char* args) {
    if (!args || !*args) {
        kprintf(ERROR, "Usage: cat <file>\n");
        return;
    }
    
    struct vfs_node* file = vfs_open(args, FS_READ);
    if (!file) {
        kprintf(ERROR, "Failed to open file\n");
        return;
    }
    
    uint8_t buffer[1024];
    uint32_t bytes_read;
    while ((bytes_read = vfs_read(file, 0, sizeof(buffer), buffer)) > 0) {
        for (uint32_t i = 0; i < bytes_read; i++) {
            kprintf(CLI, "%c", buffer[i]);
        }
    }
    
    vfs_close(file);
}

// CLI state
static char _input_buffer[CLI_BUFFER_SIZE];
static size_t _buffer_pos = 0;
static char _cmd_buffer[256];  // Static buffer for command name

// Process a command line
static void process_command(const char* line) {
    if (!line || !*line) return;

    // Find command and arguments
    const char* cmd = line;
    const char* args = NULL;
    
    // Skip leading spaces
    while (*cmd == ' ') cmd++;
    if (!*cmd) return;  // Empty line after skipping spaces
    
    // Find the end of the command (first space or null terminator)
    const char* cmd_end = cmd;
    while (*cmd_end && *cmd_end != ' ' && *cmd_end != '\0') cmd_end++;
    
    // Find the start of arguments (skip spaces)
    args = cmd_end;
    while (*args == ' ') args++;
    
    // Copy command to static buffer
    size_t cmd_len = cmd_end - cmd;
    if (cmd_len >= sizeof(_cmd_buffer)) {
        cmd_len = sizeof(_cmd_buffer) - 1;
    }
    strncpy(_cmd_buffer, cmd, cmd_len);
    _cmd_buffer[cmd_len] = '\0';
    
    // Find and execute command
    for (const struct Command* cmd_ptr = commands; cmd_ptr->name; cmd_ptr++) {
        if (strcmp(_cmd_buffer, cmd_ptr->name) == 0) {
            // Pass the arguments as-is, including any spaces
            cmd_ptr->handler(*args ? args : NULL);
            return;
        }
    }

    // Print error message
    kprintf(ERROR, "Unknown command: '%s'\n", _cmd_buffer);
    kprintf(CLI, "Type 'help' for a list of available commands\n");
}

// Function to get the current prompt
static void get_prompt(char* buffer, size_t size) {
    // Get current working directory
    struct vfs_node* cwd = vfs_getcwd();
    if (!cwd) {
        strncat(buffer, "> ", size);
        return;
    }

    // Start with empty string
    buffer[0] = '\0';
    
    // Build path string by traversing up the tree
    struct vfs_node* current = cwd;
    char temp[256] = "";
    
    while (current && current->name[0] != '\0') {
        strncpy(temp, buffer, sizeof(temp));
        buffer[0] = '/';
        buffer[1] = '\0';
        strncat(buffer, current->name, size);
        strncat(buffer, temp, size);
        current = current->parent;
    }

    // If buffer is empty (root directory), just show /
    if (buffer[0] == '\0') {
        strncpy(buffer, "", size);
    }

    // Add the prompt suffix
    strncat(buffer, "> ", size);
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
            // Update prompt with current directory
            char prompt[256];
            get_prompt(prompt, sizeof(prompt));
            kprintf(CLI, "%s", prompt);
            return true;
        default:
            return false;
    }
}

// Main CLI loop
void cli_run(void) {
    cmd_clear(NULL);

    kprintf(CLI, "\nWelcome to ZenOS\n");
    // Show initial prompt with current directory
    char prompt[256];
    get_prompt(prompt, sizeof(prompt));
    kprintf(CLI, "%s", prompt);

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
