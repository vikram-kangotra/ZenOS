global loader

extern kmain

section .text
loader:
    mov eax, 0xCAFEBABE
    mov esp, kernel_stack + KERNEL_STACK_SIZE

    call kmain

    hlt

KERNEL_STACK_SIZE equ 4096

section .bss
align 8
kernel_stack:
    resb KERNEL_STACK_SIZE
