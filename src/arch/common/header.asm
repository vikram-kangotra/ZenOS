MAGIC_NUMBER equ 0xe85250d6
FLAGS        equ 0x0
CHECKSUM     equ ~MAGIC_NUMBER

section .multiboot_header
header_start:
    dd MAGIC_NUMBER
    dd FLAGS
    dd header_end - header_start
    dd 0x100000000 - (MAGIC_NUMBER + FLAGS + header_end - header_start)

    ; end tag
    dw 0
    dw 0
    dd 0
header_end:
