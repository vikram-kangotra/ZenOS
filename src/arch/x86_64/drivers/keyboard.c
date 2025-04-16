#include "arch/x86_64/interrupt/isr.h"
#include "arch/x86_64/interrupt/pic.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/io.h"
#include "stdbool.h"
#include "stddef.h"
#include "drivers/keyboard.h"

// Keyboard ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

// Special keys
#define KEY_ESCAPE    0x01
#define KEY_BACKSPACE 0x0E
#define KEY_TAB       0x0F
#define KEY_ENTER     0x1C
#define KEY_LCTRL     0x1D
#define KEY_LSHIFT    0x2A
#define KEY_RSHIFT    0x36
#define KEY_LALT      0x38
#define KEY_CAPSLOCK  0x3A

// Keyboard buffer size
#define KEYBOARD_BUFFER_SIZE 256

// Circular buffer for keyboard input
static char _keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static size_t _buffer_head = 0;
static size_t _buffer_tail = 0;
static size_t _buffer_count = 0;

// Add a character to the buffer
static bool keyboard_buffer_put(char c) {
    if (_buffer_count >= KEYBOARD_BUFFER_SIZE) {
        return false;  // Buffer full
    }
    
    _keyboard_buffer[_buffer_head] = c;
    _buffer_head = (_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    _buffer_count++;
    return true;
}

// Get a character from the buffer
char keyboard_buffer_get(void) {
    if (_buffer_count == 0) {
        return 0;  // Buffer empty
    }
    
    char c = _keyboard_buffer[_buffer_tail];
    _buffer_tail = (_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    _buffer_count--;
    return c;
}

// Check if buffer is empty
bool keyboard_buffer_empty(void) {
    return _buffer_count == 0;
}

// Check if buffer is full
bool keyboard_buffer_full(void) {
    return _buffer_count >= KEYBOARD_BUFFER_SIZE;
}

// Get number of characters in buffer
size_t keyboard_buffer_size(void) {
    return _buffer_count;
}

// Key state tracking
static bool _key_states[256] = {0};
static bool _shift_pressed = false;
static bool _ctrl_pressed = false;
static bool _alt_pressed = false;
static bool _caps_lock = false;

// Convert scancode to ASCII with shift/caps handling
static char scancode_to_ascii(uint8_t scancode, bool shift) {
    static const char ascii_table[] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',
        'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
    };

    static const char ascii_table_shift[] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z',
        'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
    };

    if (scancode > 0 && scancode < sizeof(ascii_table)) {
        return shift ? ascii_table_shift[scancode] : ascii_table[scancode];
    }
    return 0;
}

// Check if a key is pressed
bool is_key_pressed(uint8_t scancode) {
    return _key_states[scancode];
}

// Get keyboard modifiers
bool is_shift_pressed(void) { return _shift_pressed; }
bool is_ctrl_pressed(void) { return _ctrl_pressed; }
bool is_alt_pressed(void) { return _alt_pressed; }
bool is_caps_lock(void) { return _caps_lock; }

__attribute__((interrupt))
void irq_keyboard_handler(struct InterruptStackFrame* frame) {
    (void) frame;

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    bool key_released = scancode & 0x80;
    scancode &= 0x7F;  // Remove release bit

    // Update key state
    _key_states[scancode] = !key_released;

    // Handle modifier keys
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            _shift_pressed = !key_released;
            break;
        case KEY_LCTRL:
            _ctrl_pressed = !key_released;
            break;
        case KEY_LALT:
            _alt_pressed = !key_released;
            break;
        case KEY_CAPSLOCK:
            if (!key_released) {
                _caps_lock = !_caps_lock;
            }
            break;
    }

    // Only process key press (not release) and non-modifier keys
    if (!key_released && scancode != KEY_LSHIFT && scancode != KEY_RSHIFT && 
        scancode != KEY_LCTRL && scancode != KEY_LALT && scancode != KEY_CAPSLOCK) {
        
        bool shift = _shift_pressed ^ _caps_lock;  // XOR for caps lock effect
        char key = scancode_to_ascii(scancode, shift);
        
        if (key) {
            // Store in buffer instead of printing
            if (!keyboard_buffer_put(key)) {
                // Buffer full - could add error handling here
                kprintf(ERROR, "Keyboard buffer full!\n");
            }
        }
    }

    pic_eoi(0x21);
}

// Blocking read function that waits for keyboard input
char keyboard_read_blocking(void) {
    while (keyboard_buffer_empty()) {
        // Enable interrupts while waiting
        asm volatile("sti");
        // Halt CPU to save power while waiting
        asm volatile("hlt");
        // Disable interrupts before checking buffer again
        asm volatile("cli");
    }
    
    // Re-enable interrupts before returning
    asm volatile("sti");
    return keyboard_buffer_get();
}