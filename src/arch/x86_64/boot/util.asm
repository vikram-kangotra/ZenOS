extern memset
memset:
    ; Ensure n is not zero
    test rdx, rdx
    jz .done       ; If n is 0, just return

    ; Set up the registers for the operation
    mov al, sil    ; Move the value c into AL (the byte to set)
    mov rcx, rdx   ; Move n into RCX (count of bytes)

    ; Fill memory using REPE STOSB
    ; This will fill the memory at the address in RDI with the value in AL, repeated RCX times
    cld            ; Clear the direction flag to ensure forward movement
    rep stosb      ; Repeat storing the value in AL into the address pointed to by RDI

.done:
    ret            ; Return from the function
