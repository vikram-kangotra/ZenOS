#include "arch/x86_64/interrupt/isr.h"
#include "kernel/kprintf.h"
#include "kernel/mm/vmm.h"
#include "arch/x86_64/asm.h"

void print_interrupt_stack_frame(struct InterruptStackFrame* frame) {

    kprintf(ERROR, "RIP: %p\n", frame->rip);
    kprintf(ERROR, "CS: 0x%x\n", frame->cs);
    kprintf(ERROR, "RFLAGS: 0x%x\n", frame->rflags);
    kprintf(ERROR, "RSP: %p\n", frame->rsp);
    kprintf(ERROR, "SS: 0x%x\n", frame->ss);
}

__attribute__((interrupt))
void isr_default_handler(struct InterruptStackFrame* frame) {

    (void) frame;

    kprintf(ERROR, "Unhandled interrupt called\n");
    asm("cli; hlt");
    while (1);
}

void default_handler(struct InterruptStackFrame* frame, uint64_t error_code) {
    if (error_code != 0) {
        kprintf(ERROR, "Error Code: %d\n", error_code);
    }

    print_interrupt_stack_frame(frame);

    asm("cli; hlt");
    while(1);
}

#define define_exception(isr_func) \
    __attribute__((interrupt)) \
    void isr_func(struct InterruptStackFrame* frame, uint64_t error_code) { \
        kprintf(ERROR, "%s\n", __func__); \
        default_handler(frame, error_code); \
    }

#define define_exception_no_code(isr_func) \
    __attribute__((interrupt)) \
    void isr_func(struct InterruptStackFrame* frame) { \
        kprintf(ERROR, "%s\n", __func__); \
        default_handler(frame, 0); \
    }

define_exception_no_code(isr_divide_error)
define_exception_no_code(isr_debug)
define_exception_no_code(isr_non_maskable_interrupt)
define_exception_no_code(isr_breakpoint)
define_exception_no_code(isr_overflow)
define_exception_no_code(isr_bound_range_exceeded)
define_exception_no_code(isr_inavlid_opcode)
define_exception_no_code(isr_device_not_found)
define_exception(isr_double_fault)
define_exception_no_code(isr_coprocess_segment_overrun)
define_exception(isr_invalid_tss)
define_exception(isr_segment_not_present)
define_exception(isr_stack_segment_fault)
define_exception(isr_general_protection_fault)
define_exception_no_code(isr_reserved)
define_exception_no_code(isr_x87_floating_point_exception)
define_exception(isr_alignment_check)
define_exception_no_code(isr_machine_check)
define_exception_no_code(isr_simd_floating_point_exception)
define_exception_no_code(isr_virtualization_exception)
define_exception(isr_control_protection_exception)
define_exception_no_code(isr_reserved1);
define_exception_no_code(isr_reserved2);
define_exception_no_code(isr_reserved3);
define_exception_no_code(isr_reserved4);
define_exception_no_code(isr_reserved5);
define_exception_no_code(isr_reserved6);
define_exception_no_code(isr_reserved7);
define_exception(isr_hypervisor_injection_exception)
define_exception(isr_vmm_communication_exception)
define_exception_no_code(isr_security_exception)

__attribute__((interrupt))
void isr_page_fault(struct InterruptStackFrame* frame, uint64_t error_code) {
<<<<<<< HEAD

    (void) frame;

    kprintf(ERROR, "Page Fault\n");

    kprintf(ERROR, "Error Code: %d\n", error_code);

    uintptr_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

    kprintf(ERROR, "Faulting Address: %d", faulting_address);

    asm("cli; hlt");
    while(1);
=======
    uintptr_t faulting_address = get_faulting_address();
    
    kprintf(ERROR, "Faulting Address: %p\n", faulting_address);
    
    uintptr_t virtual_address = faulting_address & ~(PAGE_SIZE - 1);
    
    uintptr_t new_frame = (uintptr_t) buddy_alloc(PAGE_SIZE);

    if (!new_frame) {
        default_handler(frame, error_code);
        return;
    }
    
    map_virtual_to_physical(virtual_address, new_frame);
>>>>>>> refs/remotes/origin/main
}
