; 引导扇区 FAT12

%INCLUDE "gdt.asm" ; 段描述表定义

[BITS 16]
org 0x7c00 ; 加载地址偏移 参考http://blog.csdn.net/u011542994/article/details/46707815

BS_jmpBoot      jmp   entry
                db    0x90
BS_OEMName      db    "CCOSV587"        ; OEM name / 8 B
BPB_BytsPerSec  dw    512               ; 一个扇区512字节 
BPB_SecPerClus  db    1                 ; 每个簇一个扇区
BPB_RsvdSecCnt  dw    1                 ; 保留扇区数, 必须为1
BPB_NumFATs     db    2                 ; FAT表份数
BPB_RootEntCnt  dw    224               ; 根目录项数
BPB_TotSec16    dw    2880              ; RolSec16, 总扇区数
BPB_Media       db    0xf0              ; 介质种类: 移动介质
BPB_FATSz16     dw    9                 ; FATSz16 分区表占用扇区数
BPB_SecPerTrk   dw    18                ; SecPerTrk, 磁盘 
BPB_NumHeads    dw    2                 ; 磁头数    
BPB_HiddSec     dd    0                 ; HiddSec
BPB_TotSec32    dd    2880              ; 卡容量
BS_DrvNum       db    0                 ; DvcNum
BS_Reserved1    db    0                 ; NT保留    
BS_BootSig      db    0x29              ; BootSig扩展引导标记
BS_VolD         dd    0xffffffff        ; VolID 
BS_VolLab       db    "FLOPPYCDDS "     ; 卷标
BS_FileSysType  db    "FAT12   "        ; FilesysType

times 18 db 0

_print16:
loop:
    lodsb   ; ds:si -> al
    or al,al
    jz done
    mov ah,0x0e
    mov bx,15        
    int 0x10        ; 打印字符
    jmp loop
done:
    ret

;============================================================
; 入口
entry:
    mov ax,0        
    mov ss,ax
    mov sp,0x7c00
    mov ds,ax
    mov es,ax   ; bios interrupt expects ds

    ; shift to text mode, 16 color 80*25
    ; 参考自http://blog.csdn.net/hua19880705/article/details/8125706
    ;      http://www.cnblogs.com/magic-cube/archive/2011/10/19/2217676.html
    mov ah,0x0
    mov al,0x03  ; 设置模式 16色 80x25矩阵
    int 0x10     ; 设置颜色

    mov si, msg_boot
    call _print16

;============================================================
; 获取内存布局
; http://blog.csdn.net/yes_life/article/details/6839453

ARD_ENTRY equ 0x500
ARD_COUNT equ 0x400

get_mem_map:
    mov dword [ARD_COUNT], 0
    mov ax, 0
    mov es, ax
    xor ebx, ebx           ; If this is the first call, EBX must contain zero.
    mov di, ARD_ENTRY      ; ES:DI-> Buffer Pointer

get_mem_map_loop:
    mov eax, 0xe820        ; code
    mov ecx, 20            ; Buffer Size, min = 20
    mov edx, 0x534D4150    ; Signature, edx = "SMAP"
    int 0x15
    jc get_mem_map_fail    ; Non-Carry - indicates no error
    add di, 20             ; Buffer Pointer += 20(sizeof structure)
    inc dword [ARD_COUNT]  ; Inc ADR count
    cmp ebx, 0             ; Anything else?
    jne get_mem_map_loop 
    jmp get_mem_map_ok

get_mem_map_fail:
    mov si, msg_get_mem_map_err
    call _print16
    hlt

get_mem_map_ok:

;============================================================
; 从软盘中读取内核代码
; 参考http://blog.chinaunix.net/uid-20496675-id-1664077.html
;     http://chuanwang66.iteye.com/blog/1678952

; 读磁盘时，将读到的扇区放到[es:bx]开始的内存中
; 写磁盘时，将[es:bx]开始的一个扇区写到磁盘上
; 这两处，[es:bx]都称为 数据缓冲区

; read 20 sector (360 KB) form floppy
loadloader:      
    mov bx,0    
    mov ax,0x0800 
    mov es,ax   ; [es:bx] buffer address point -> 0x8000 将读取数据存放至0x8000
    mov cl,2    ; 扇区 Sector
    mov ch,0    ; 磁道 Track
    mov dh,0    ; 盘面 Cylinder
    mov dl,0    ; 驱动器号 driver a:
    ; kernel locates after bootloader, which is the second sector

readloop:
    mov si,0    ; 错误计数 err counter

retry:
    mov ah,0x02 ; int 0x13 ah = 0x02 read sector form dirve
    mov al,1    ; read 1 sector 
    int 0x13    ; 读取磁道1
    jnc next    ; 没有错误则继续读取
    add si,1
    cmp si,5    ; 累计错误出现5次就报错
    jae error
    mov ah,0
    mov dl,0    ; driver a
    int 0x13    ; 复位 reset
    jmp next 

next: 
    mov ax,es
    add ax,0x20 ; 一个扇区是512B=0x200，es是段，故再除以16，得到0x20
    mov es,ax

    add cl,1    ; 读下一个扇区 sector + 1
    cmp cl,18   ; 18 sector 如果读满了所有18个扇区，就
    jbe readloop

    mov cl,1
    add dh,1    ; 盘面 + 1
    cmp dh,1
    jbe readloop

    mov dh,0
    add ch,1    ; 磁道 + 1
    cmp ch,20   ; 只读取20个磁道共360KB
    jbe readloop
    jmp succ

error:        
    mov  si,msg_err ; 报错
    call _print16
    jmp $           ; halt

succ:    
    mov si,msg_succ ; 读取成功
    call _print16

    ; fill and load GDTR 读取全局描述符表寄存器
    ; 参考http://x86.renejeschke.de/html/file_module_x86_id_156.html
    xor eax,eax
    mov ax,ds
    shl eax,4
    add eax,GDT                 ; eax <- gdt base 
    mov dword [GdtPtr+2],eax    ; [GdtPtr + 2] <- gdt base 

    lgdt [GdtPtr]
    cli

    ; turn on A20 line
    ; 参考 http://blog.csdn.net/yunsongice/article/details/6110648
    in al,0x92
    or al,00000010b
    out 0x92,al

    ; 切换到保护模式 shift to protect mode  
    mov eax,cr0
    or eax,1
    mov cr0,eax

    ; special, clear pipe-line and jump
    ; 前面读取软盘数据到0x8000处，现在跳转至0x8000 
    jmp dword Selec_Code32_R0:0x8000 ; 自动切换段寄存器

msg_boot:
    db "[Bootsector] loading...",13,10,0 ; 13 10(0x0D 0x0A)是'\r \n'
msg_err:
    db "[Bootsector] error",13,10,0
msg_succ:
    db "[Bootsector] ok",13,10,0
msg_temp:
    db 0,0,0
msg_get_mem_map_err:
    db "[Bootsector] failed",0

GDT: ; 全局描述符表
DESC_NULL:        Descriptor 0,       0,            0                         ; null
DESC_CODE32_R0:   Descriptor 0,       0xfffff - 1,  DA_C+DA_32+DA_LIMIT_4K    ; uncomfirm 
DESC_DATA_R0:     Descriptor 0,       0xfffff - 1,  DA_DRW+DA_32+DA_LIMIT_4K  ; uncomfirm  ; 4G seg 
DESC_VIDEO_R0:    Descriptor 0xb8000, 0xffff,       DA_DRW+DA_32              ; vram 

GdtLen  equ $ - GDT     ; GDT len
GdtPtr  dw  GdtLen - 1  ; GDT limit
        dd  0           ; GDT Base

; GDT Selector 
Selec_Code32_R0 equ     DESC_CODE32_R0 - DESC_NULL
Selec_Data_R0   equ     DESC_DATA_R0   - DESC_NULL 
Selec_Video_R0  equ     DESC_VIDEO_R0  - DESC_NULL

times 510 - ($-$$) db 0 ; 填充零
db 0x55, 0xaa
