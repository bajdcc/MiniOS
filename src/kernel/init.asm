; init proc
align 4

[bits 32]
[section .text]
[global __init_start]
[global __init_end]

[extern user_main]

__init_start:
    nop
    nop
    push user_main
    pop eax
    call eax
    jmp $

__init_end:
