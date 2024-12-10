#include "kernel/shell.h"
#include "drivers/keyboard.h"
#include "drivers/gfx/gfx.h"
#include "kernel/kprintf.h"
#include "string.h"
#include "drivers/rtc.h"

#define INPUT_BUFFER_SIZE 256

static void display_prompt() {
    kprintf(DEBUG, "> ");
}

static void process_command(const char* input_buffer, struct multiboot_tag_framebuffer* fb_info) {
    if (strcmp(input_buffer, "about") == 0) {
        zenos();
        kprintf(DEBUG, "\n");
        kprintf(DEBUG, "ZenOS v1.0\nBuilt with love and low-level magic.\n");
        kprintf(DEBUG, "ZenOS is a WebAssembly based Operating System focused\n");
        kprintf(DEBUG, "on high security in embedded Systems\n");
        kprintf(DEBUG, "\n");
        kprintf(DEBUG, "Authors: Vikram Kangotra, Disha Baghel, Akshita Singh\n");
    } else if (strcmp(input_buffer, "clear") == 0) {
        clear_screen(fb_info, GFX_COLOR_BLACK(fb_info));
    } else if (strcmp(input_buffer, "time") == 0) {
        rtc_print_time();
        kprintf(DEBUG, "\n");
    } else if (strcmp(input_buffer, "zen") == 0) {
        kprintf(DEBUG, "The journey is the reward. Stay calm, code on.\n");
    } else if (strcmp(input_buffer, "help") == 0) {
        kprintf(DEBUG, "Available commands:\n");
        kprintf(DEBUG, "  about  - Show information about the OS\n");
        kprintf(DEBUG, "  clear  - Clear the screen\n");
        kprintf(DEBUG, "  time   - Show current time\n");
        kprintf(DEBUG, "  zen    - Show current time\n");
        kprintf(DEBUG, "  help   - Display this help message\n");
    } else {
        kprintf(ERROR, "Unknown command: %s\n", input_buffer);
    }
}

void shell() {
    char input_buffer[INPUT_BUFFER_SIZE];
    size_t input_index = 0;

    struct multiboot_tag_framebuffer* fb_info = get_framebuffer_info();

    for (;;) {
        
        display_prompt();
        input_index = 0;

        char key;
        while (1) {
            key = buffer_read();
            if (key == 0) {
                continue;
            }

            if (key == '\n') { 
                input_buffer[input_index] = '\0'; 
                break;
            }

            if (key == '\b') { 
                if (input_index > 0) {
                    input_index--;
                    kprintf(DEBUG, "\b \b");
                }
            } else if (input_index < INPUT_BUFFER_SIZE - 1) { 
                input_buffer[input_index++] = key;
                kprintf(DEBUG, "%c", key);
            }
        }

        
        if (input_index == 0) {
            continue;
        }
 
        kprintf(DEBUG, "\n");
        process_command(input_buffer, fb_info);
    }
}
