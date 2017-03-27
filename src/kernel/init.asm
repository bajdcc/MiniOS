; init proc
align 4

[bits 32]
[section .text]
[global __init_start]
[global __init_end]

[extern sys_tasks0]
[extern sys_ticks]

; 注意：由于链接器的原因，这里的sys_tasks0和sys_ticks都是地址0,正在解决这个bug

__init_start:
    nop
    nop
    mov eax, 1 ; fork
    int 0x80 ; syscall
    cmp eax, 0
    jz child

    call delay
    push sys_tasks0
    pop eax
    call eax ; call sys_tasks0

    call delay
    mov eax, 5 ; wait
    int 0x80 ; syscall
    jmp $

child:
    call delay
    push sys_ticks ; SYSCALL(0x80) | IPC(0x7) | SYS_TICKS(2)
    pop eax
    call eax ; call sys_ticks
    call delay
    push eax
    mov eax, 3 ; exec
    int 0x80 ; syscall

    call delay
    jmp child

    call delay
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
