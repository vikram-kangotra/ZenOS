#include "interrupts/exceptions.h"
#include "print.h"
#include "kernel/serial.h"
#include <stdarg.h>

void error(const char* err, ...) {
    va_list args;

    va_start(args, err);
    va_kprintf(err, args);
    va_end(args);

    va_start(args, err);
    va_write_serial(err, args);
    va_end(args);
}

void print_interrupt_frame(struct InterruptStackFrame *frame) {
    error("Interrupt frame at %x \n", (unsigned long)frame);
    error("Instruction pointer: %x \n", frame->instruction_pointer);
    error("Code segment: %x \n", frame->code_segment);
    error("CPU flags: %x \n", frame->cpu_flags);
    error("Stack pointer: %x \n", frame->stack_pointer);
    error("Stack segment: %x", frame->stack_segment);
}

void divide_by_zero_handler(struct InterruptStackFrame *frame) {
    error("Divide by zero\n");
    print_interrupt_frame(frame);
    while (1);
}

void invalid_opcode_handler(struct InterruptStackFrame *frame) {
    error("Invalid opcode\n");
    print_interrupt_frame(frame);
    while (1);
}

void breakpoint_handler(struct InterruptStackFrame *frame) {
    error("Breakpoint\n");
    print_interrupt_frame(frame);
    while (1);
}

void double_fault_handler(struct InterruptStackFrame *frame) {
    error("Double fault\n");
    print_interrupt_frame(frame);
    while (1);
}

void general_protection_fault_handler(struct InterruptStackFrame *frame) {
    error("General protection fault\n");
    print_interrupt_frame(frame);
    while (1);
}

void page_fault_handler(struct InterruptStackFrame *frame) {
    error("Page fault\n");
    print_interrupt_frame(frame);

    unsigned long cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    write_serial("Page fault at address: %x", cr2);

    while (1);
}
