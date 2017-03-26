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
    cmp eax, 0
    jz child

loop:
    mov eax, 5 ; wait
    int 0x80 ; syscall
    nop
    nop
    jmp loop
    jmp $

child:
    mov eax, 3 ; exec
    int 0x80 ; syscall
    mov eax, 2 ; exit
    int 0x80 ; syscall
    jmp $

__init_end:
