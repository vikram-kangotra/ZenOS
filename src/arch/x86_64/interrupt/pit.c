#include "arch/x86_64/interrupt/pit.h"
#include "arch/x86_64/interrupt/pic.h"
#include "arch/x86_64/io.h"
#include "kernel/kprintf.h"

#define PIT_FREQUENCY 1193182

#define	PIT_REG_COUNTER0    0x40
#define	PIT_REG_COMMAND		0x43
 
uint32_t _pit_ticks=0;

void pit_set_frequency(uint32_t frequency, uint8_t mode) {

    if (frequency == 0) 
        return;

    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / frequency);

	uint8_t ocw=0;
	ocw = (ocw & ~PIT_OCW_MASK_MODE) | mode;
	ocw = (ocw & ~PIT_OCW_MASK_RL) | PIT_OCW_RL_DATA;
	ocw = (ocw & ~PIT_OCW_MASK_COUNTER) | PIT_OCW_COUNTER_0;
    
    out(PIT_REG_COMMAND, ocw);
    
    out(PIT_REG_COUNTER0, divisor & 0xFF);
    out(PIT_REG_COUNTER0, (divisor >> 8) & 0xFF);

    _pit_ticks = 0;
}

__attribute__((interrupt))
void irq_pit_handler(struct InterruptStackFrame* frame) {

    (void) frame;

    ++_pit_ticks;

    pic_eoi(0x20);
}

void init_pit(uint32_t frequency) {
    pit_set_frequency(frequency, PIT_OCW_MODE_SQUAREWAVEGEN);
}
