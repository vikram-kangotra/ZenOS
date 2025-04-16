#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declaration
struct InterruptStackFrame;

// Keyboard buffer interface
char keyboard_buffer_get(void);
char keyboard_read_blocking(void);  // Blocking read function
bool keyboard_buffer_empty(void);
bool keyboard_buffer_full(void);
size_t keyboard_buffer_size(void);

// Key state checking
bool is_key_pressed(uint8_t scancode);
bool is_shift_pressed(void);
bool is_ctrl_pressed(void);
bool is_alt_pressed(void);
bool is_caps_lock(void);

// Interrupt handler
__attribute__((interrupt))
void irq_keyboard_handler(struct InterruptStackFrame* frame);
