#include "arch/x86_64/interrupt/pit.h"
#include "arch/x86_64/interrupt/pic.h"
#include "arch/x86_64/interrupt/idt.h"
#include "arch/x86_64/io.h"
#include "kernel/kprintf.h"

#define PIT_FREQUENCY 1193182
#define PIT_MIN_FREQ 18      // ~55ms period
#define PIT_MAX_FREQ 1193182 // 1ms period

#define PIT_REG_COUNTER0    0x40
#define PIT_REG_COMMAND     0x43

// Timer state
static uint32_t _pit_ticks = 0;
static uint32_t _pit_frequency = 0;
static bool _pit_initialized = false;

// Get current tick count
uint32_t pit_get_ticks(void) {
    return _pit_ticks;
}

// Convert ticks to milliseconds
uint32_t pit_ticks_to_ms(uint32_t ticks) {
    if (_pit_frequency == 0) return 0;
    return (ticks * 1000) / _pit_frequency;
}

// Convert milliseconds to ticks
static uint32_t ms_to_ticks(uint32_t ms) {
    if (_pit_frequency == 0) return 0;
    return (ms * _pit_frequency) / 1000;
}

// Sleep for specified milliseconds
void pit_sleep_ms(uint32_t milliseconds) {
    if (!_pit_initialized) {
        kprintf(ERROR, "PIT: Cannot sleep, timer not initialized\n");
        return;
    }

    uint32_t start_ticks = pit_get_ticks();
    uint32_t target_ticks = start_ticks + ms_to_ticks(milliseconds);

    // Wait until we reach the target tick count
    while (pit_get_ticks() < target_ticks) {
        // Enable interrupts while waiting
        asm volatile("sti");
        // Halt the CPU to save power
        asm volatile("hlt");
        // Disable interrupts to check the condition
        asm volatile("cli");
    }
}

// Stop the timer
void pit_stop(void) {
    if (!_pit_initialized) return;
    
    // Set frequency to 0 to stop the timer
    out(PIT_REG_COMMAND, 0x30); // Mode 0, binary
    out(PIT_REG_COUNTER0, 0);
    out(PIT_REG_COUNTER0, 0);
    
    _pit_initialized = false;
}

void pit_set_frequency(uint32_t frequency, uint8_t mode) {
    if (frequency < PIT_MIN_FREQ || frequency > PIT_MAX_FREQ) {
        kprintf(ERROR, "PIT: Invalid frequency %u Hz (must be between %u and %u)\n", 
                frequency, PIT_MIN_FREQ, PIT_MAX_FREQ);
        return;
    }

    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / frequency);
    if (divisor == 0) {
        kprintf(ERROR, "PIT: Frequency too high, divisor would be 0\n");
        return;
    }

    uint8_t ocw = 0;
    ocw = (ocw & ~PIT_OCW_MASK_MODE) | mode;
    ocw = (ocw & ~PIT_OCW_MASK_RL) | PIT_OCW_RL_DATA;
    ocw = (ocw & ~PIT_OCW_MASK_COUNTER) | PIT_OCW_COUNTER_0;
    
    out(PIT_REG_COMMAND, ocw);
    
    out(PIT_REG_COUNTER0, divisor & 0xFF);
    out(PIT_REG_COUNTER0, (divisor >> 8) & 0xFF);

    _pit_ticks = 0;
    _pit_frequency = frequency;
    _pit_initialized = true;
}

__attribute__((interrupt))
void irq_pit_handler(struct InterruptStackFrame* frame) {
    (void) frame;

    if (_pit_initialized) {
        ++_pit_ticks;
    }

    pic_eoi(0x20);
}

void init_pit(uint32_t frequency) {
    // Initialize the PIT with the requested frequency
    pit_set_frequency(frequency, PIT_OCW_MODE_SQUAREWAVEGEN);
    
    if (_pit_initialized) {
        kprintf(INFO, "PIT initialized at %u Hz\n", frequency);
    } else {
        kprintf(ERROR, "PIT initialization failed\n");
    }
}
