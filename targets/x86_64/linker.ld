ENTRY(loader)

SECTIONS {

    . = 1M;

    .boot ALIGN (0X1000) :
    {
        KEEP(*(.multiboot_header))
    }

    .text ALIGN (0x1000) :
    {
        *(.text)
    }

    .rodata ALIGN (0x1000) :
    {
        *(.rodata*)
    }

    .data ALIGN (0x1000) :
    {
        *(.data)
    }

    .bss ALIGN (0X1000) :
    {
        *(COMMON)
        *(.bss)
    }
}
