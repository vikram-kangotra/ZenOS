MAGIC_NUMBER equ 0xe85250d6
FLAGS        equ 0x0
CHECKSUM     equ ~MAGIC_NUMBER

section .multiboot_header
header_start:
    dd MAGIC_NUMBER
    dd FLAGS
    dd header_end - header_start
    dd 0x100000000 - (MAGIC_NUMBER + FLAGS + header_end - header_start)

    ; framebuffer tag
.framebuffer_tag_start:
    dw 5
    dw 0
    dd .framebuffer_tag_end - .framebuffer_tag_start
    dd 1920
    dd 1080
    dd 32
.framebuffer_tag_end

    ; end tag
    dw 0
    dw 0
    dd 0
header_end:
