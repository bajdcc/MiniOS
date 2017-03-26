; init proc
align 4

[bits 32]
[section .text]
[global __init_start]
[global __init_end]

__init_start:
    nop
    mov eax, 1 ; fork
    int 0x80 ; syscall
    
    jmp $

__init_end:
