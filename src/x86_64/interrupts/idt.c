#include "interrupts/idt.h"
#include "interrupts/isr.h"
#include "interrupts/exceptions.h"
#include "interrupts/pic.h"
#include "kernel/io.h"
#include "print.h"

#define IDT_ENTRIES 256
struct idt_entry idt[IDT_ENTRIES];
struct idt_ptr idtp;

extern void load_idt(uint64_t);

extern void keyboard_handler();

void idt_set_entry(int n, uint64_t handler) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector = 0x08;
    idt[n].ist = 0;
    idt[n].type_attr = 0x8E;
    idt[n].offset_mid = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero = 0;
}

void idt_init() {
    idtp.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtp.base = (uint64_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_entry(i, (uint64_t)isr_default_handler);
    }

    idt_set_entry(0, (uint64_t)divide_by_zero_handler);
    idt_set_entry(6, (uint64_t)invalid_opcode_handler);
    idt_set_entry(3, (uint64_t)breakpoint_handler);
    idt_set_entry(8, (uint64_t)double_fault_handler);
    idt_set_entry(13, (uint64_t)general_protection_fault_handler);
    idt_set_entry(14, (uint64_t)page_fault_handler);

    idt_set_entry(33, (uint64_t)keyboard_handler);

    load_idt((uint64_t)&idtp);

    pic_remap();

    enable_irq();
}

