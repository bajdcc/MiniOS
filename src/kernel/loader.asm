; loader.asm
; jmp to C kernel, achieve some function in asm 
; 

; kernel code segment selector
SEL_KERN_CODE   EQU 0x8
; kernel data segment selector
SEL_KERN_DATA   EQU 0x10
; vedio memory
SEL_KERN_VIDEO  EQU 0x18
; 用户地址起始
USER_BASE       EQU 0xc0000000

align 4

[bits 32]
[section .text]

[extern os_main]
[global start]
start:
    xor eax, eax
    mov ax, SEL_KERN_DATA
    mov ds, ax
    mov ax, SEL_KERN_DATA
    mov es, ax
    mov ax, SEL_KERN_VIDEO
    mov gs, ax
    mov ax, SEL_KERN_DATA
    mov ss, ax
    mov esp, 0x7c00 ; 联想到bootsect中的org 0x7c00

; mov the kernel to 0x100000
[extern kernstart]
[extern kernend]
    mov eax, kernend
    mov ecx, kernstart
    sub eax, ecx
    mov ecx, eax
    mov esi, 0x8000
    mov edi, 0x100000
    cld
    rep movsb
    jmp dword SEL_KERN_CODE:go

go:
    mov	edi, (160*3)+0   ; 160*50 line 3 column 1
    mov	ah, 00001100b    ; red color

    mov esi, msg 
    call print

    push 0
    jmp os_main          ; os entry

    jmp $                ; halt

print:
    add edi, 160
    push edi
    cld

loop:
    lodsb
    cmp al, 0
    je outloop
    mov	[gs:edi], ax
    add edi, 2
    jmp loop

outloop:
    pop edi
    ret

msg:
    db "=== [ OS ENTRY ] ===", 0

; ####################### 内核专用 #######################

; ********** gdt.c
[global gdt_flush]
[extern gp]

gdt_flush:      
    lgdt [gp] ; load gdt
    mov ax, SEL_KERN_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp SEL_KERN_CODE:flush2
flush2:
    ret

; ********** idt.c
[global idt_load]
[extern idtp]

idt_load:
    lidt [idtp] ; load idt
    ret

; ####################### 中断服务 #######################

; ***** 0-31号中断 exception int 0 - int 31
%macro m_fault 1
[global fault%1]
fault%1:
    cli
    ; int 8,10-14,17,30 have error code
    %if %1 != 17 && %1 != 30 && (%1 < 8 || %1 > 14)
        push 0 ; fake error code
    %endif
    push %1
    jmp _isr_stub ; 中断处理程序
%endmacro 

%assign i 0
%rep 32
    m_fault i
    %assign i i + 1
%endrep

; ***** 32-47号中断 Interrupt Request int 32 - 47
%macro m_irq 1
[global irq%1]
irq%1:
    cli
    push 0
    push %1+32
    jmp _isr_stub
%endmacro

%assign i 0
%rep 16 
    m_irq i   
%assign i i+1
%endrep

; ***** Unknown Interrupt
; 255 is a flag of unknown int
; so please note that we can't use it 
[global isr_unknown]
isr_unknown:
    cli
    push 0
    push 255
    jmp _isr_stub


; ####################### 中断服务例程 #######################

; kernel/isr.c 
[global _isr_stub_ret]
[extern isr_stub]
; a common ISR func, sava the context of CPU 
; call C func to process fault
; at last restore stack frame
_isr_stub:
    ; 保存现场
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, SEL_KERN_DATA ; 内核数据段
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov eax, esp
    push eax

    mov eax, isr_stub
    call eax ; 中断处理
    pop eax

_isr_stub_ret:
    ; 恢复现场
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret


; ####################### 进程切换 #######################
; kernel/proc.c
; void _context_switch(struct context **old, struct context* new);
; http://wiki.osdev.org/Context_Switching

[global _context_switch]
_context_switch:
    mov eax, [esp + 4]  ; old
    mov edx, [esp + 8]  ; new

    ; eip has been save when call _context_switch
    push ebp
    push ebx
    push esi
    push edi

    ; switch stack
    mov [eax], esp      ; save esp
    mov esp, edx

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret


; ####################### 系统调用 #######################
; int 0x80
[global _syscall]
_syscall:
    cli
    push 0
    push 0x80
    jmp _isr_stub

; defines
sys_fork    EQU 1
sys_exit    EQU 2
sys_exec    EQU 3
sys_sleep   EQU 4

; syscall macro
%macro syscall 1
[global %1]
%1:
    mov eax, sys%1
    int 0x80
    ret
%endmacro

syscall _fork
syscall _exit
syscall _exec
syscall _sleep