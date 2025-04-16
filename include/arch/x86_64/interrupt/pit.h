#pragma once

#include "arch/x86_64/interrupt/isr.h"
#include "stdint.h"
#include "stdbool.h"

// PIT Operating Command Word (OCW) masks
#define PIT_OCW_MASK_BINCOUNT        0x01    // 00000001
#define PIT_OCW_MASK_MODE            0x0E    // 00001110
#define PIT_OCW_MASK_RL              0x30    // 00110000
#define PIT_OCW_MASK_COUNTER         0xC0    // 11000000

// PIT Operating Command Word (OCW) values
#define PIT_OCW_BINCOUNT_BINARY      0x00    // Use when setting PIT_OCW_MASK_BINCOUNT
#define PIT_OCW_BINCOUNT_BCD         0x01
#define PIT_OCW_MODE_TERMINALCOUNT   0x00    // Use when setting PIT_OCW_MASK_MODE
#define PIT_OCW_MODE_ONESHOT         0x02
#define PIT_OCW_MODE_RATEGEN         0x04
#define PIT_OCW_MODE_SQUAREWAVEGEN   0x06
#define PIT_OCW_MODE_SOFTWARETRIG    0x08
#define PIT_OCW_MODE_HARDWARETRIG    0x0A
#define PIT_OCW_RL_LATCH             0x00    // Use when setting PIT_OCW_MASK_RL
#define PIT_OCW_RL_LSBONLY           0x10
#define PIT_OCW_RL_MSBONLY           0x20
#define PIT_OCW_RL_DATA              0x30
#define PIT_OCW_COUNTER_0            0x00    // Use when setting PIT_OCW_MASK_COUNTER
#define PIT_OCW_COUNTER_1            0x40
#define PIT_OCW_COUNTER_2            0x80

// PIT frequency limits
#define PIT_MIN_FREQ 18      // ~55ms period
#define PIT_MAX_FREQ 1193182 // 1ms period

// PIT functions
void irq_pit_handler(struct InterruptStackFrame* frame);
void init_pit(uint32_t frequency);
void pit_set_frequency(uint32_t frequency, uint8_t mode);
void pit_stop(void);
uint32_t pit_ticks_to_ms(uint32_t ticks);
uint32_t pit_get_ticks(void);
void pit_sleep_ms(uint32_t milliseconds);
