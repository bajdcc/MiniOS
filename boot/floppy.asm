; 软盘 - 引导扇区
INCBIN "bootsect.bin"  ; 引导区代码
INCBIN "../bin/kernel" ; 内核代码

; 其他空间填充零
; 软盘大小：80（磁道）x 18（扇区）x 512 bytes(扇区的大小) x 2（双面）
;     = 1440 x 1024 bytes = 1440 KB = 1.44MB
times 80*18*2*512 - ($-$$) db 0 ; 80 (Track / sectors) * 18 (Sector / head) * 2 Cylinder * 512 byte
