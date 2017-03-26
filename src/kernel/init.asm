; init proc
align 4

[bits 32]
[section .text]
[global __init_start]
[global __init_end]

__init_start:
    nop
.fork:
    mov eax, 1 ; fork
    int 0x80 ; syscall
    cmp eax, 0
    jz child
    call delay
    mov eax, 5 ; wait
    int 0x80 ; syscall
    call delay
    loop .fork

child:
    mov eax, 3 ; exec
    int 0x80 ; syscall
    mov eax, 2 ; exit
    int 0x80 ; syscall
    jmp $

delay:
    push ecx
    mov ecx, 0x100000
    loop $
    pop ecx
    ret

__init_end:
