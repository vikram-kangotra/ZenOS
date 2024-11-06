#pragma once

#define PIC1        0x20   /* Master PIC */
#define PIC2        0xA0   /* Slave PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2 + 1)
#define PIC_EOI     0x20

void pic_remap();
void enable_irq();
