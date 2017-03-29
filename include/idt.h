#ifndef __IDT_H 
#define __IDT_H 

#include <type.h>
#include <gdt.h>

/* 参考: http://wiki.osdev.org/IDT */

// 中断描述符表结构 http://blog.csdn.net/fwqcuc/article/details/5855460
// base: 存储中断处理函数/中断向量的地址
// selector: 中断向量选择子，其选择子的DPL必须是零(即最高级ring0) http://blog.csdn.net/better0332/article/details/3416749
// gate: reference http://wiki.osdev.org/IDT
//       0b0101	0x5	5	80386 32 bit task gate
//       0b0110	0x6	6	80286 16-bit interrupt gate
//       0b0111	0x7	7	80286 16-bit trap gate
//       0b1110	0xE	14	80386 32-bit interrupt gate
//       0b1111	0xF	15	80386 32-bit trap gate
// flags: 
//       Storage Segment(0) Set to 0 for interrupt gates (see above).
//       DPL(1-2) Gate call protection. 0-3的特权等级
//       P(3) Set to 0 for unused interrupts.
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t always0;        // must be 0
    unsigned gate_type: 4;  // gate type
    unsigned flags: 4;      // S(0) DPL(1-2) P(3)
    uint16_t base_high;
} __attribute__((packed));

// IDTR
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// 任务状态段 task state segment http://wiki.osdev.org/TSS
// The only interesting fields are SS0 and ESP0.
//   SS0 gets the kernel datasegment descriptor (e.g. 0x10 if the third entry in your GDT describes your kernel's data)
//   ESP0 gets the value the stack-pointer shall get at a system call
//   IOPB may get the value sizeof(TSS) (which is 104) if you don't plan to use this io-bitmap further (according to mystran in http://forum.osdev.org/viewtopic.php?t=13678)

// http://blog.csdn.net/huybin_wang/article/details/2161886
// TSS的使用是为了解决调用门中特权级变换时堆栈发生的变化

// http://www.kancloud.cn/wizardforcel/intel-80386-ref-manual/123838
/*
    TSS 状态段由两部分组成：
    1、 动态部分（处理器在每次任务切换时会设置这些字段值）
        通用寄存器（EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI）
        段寄存器（ES，CS，SS，DS，FS，GS）
        状态寄存器（EFLAGS）
        指令指针（EIP）
        前一个执行的任务的TSS段的选择子（只有当要返回时才更新）
    2、 静态字段（处理器读取，但从不更改）
        任务的LDT选择子
        页目录基址寄存器（PDBR）（当启用分页时，只读）
        内层堆栈指针，特权级0-2
        T-位，指示了处理器在任务切换时是否引发一个调试异常
        I/O 位图基址
*/
struct tss_entry {
    uint32_t link;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldtr;
    uint16_t padding1;
    uint16_t iopb_off;
} __attribute__ ((packed));

#define NIDT 256            // 中断描述符表大小 Interrupt Descriptor Table

/* 386 32-bit gata type */
#define GATE_TASK 0x5
#define GATE_INT  0xe
#define GATE_TRAP 0xf

#define IDT_SS   0x1        // 存储段 store segment
#define IDT_DPL_KERN 0x0    // 内核特权级 descriptor privilege level
#define IDT_DPL_SYST 0x2    // 系统服务特权级 descriptor privilege level
#define IDT_DPL_USER 0x6    // 用户特权级 descriptor privilege level
#define IDT_PR  0x8         // 有效位 present in memory

// 初始化中断向量表
void idt_init();

// 设置中断向量
void idt_install(uint8_t num, uint32_t base, uint16_t selector, uint8_t gate, uint8_t flags);

// 设置任务状态段
void tss_install();

// 设置TSS为当前进程堆栈
extern void tss_reset();

#endif
