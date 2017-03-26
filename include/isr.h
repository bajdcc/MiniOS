#ifndef __ISR_H 
#define __ISR_H 

#include <type.h>

/* 参考: http://wiki.osdev.org/Interrupt_Service_Routines */

// IDT.h中只是设置中断向量，而这里是设置中断服务例程ISR，是处理中断的
// 中断处理程序和中断服务例程不同
// 所有中断由中断处理程序处理，再分派到各个中断服务例程中

#define ISR_IRQ0    32 // 处理32-47的中断
#define ISR_NIRQ    16
#define ISR_SYSCALL 0x80 // 系统调用中断，目前还没用到它
#define ISR_UNKNOWN 255

// 中断时要保存的寄存器数据
struct interrupt_frame {
    /* segment registers */
    uint32_t gs;    // 16 bits
    uint32_t fs;    // 16 bits
    uint32_t es;    // 16 bits
    uint32_t ds;    // 16 bits

    /* registers save by pusha */
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t ret_addr; // 返回地址
    uint32_t int_no;

    /* save by `int` instruction */
    uint32_t eip;
    uint32_t cs;    // 16 bits
    uint32_t eflags;
    uint32_t user_esp;
    uint32_t ss;    // 16 bits
};    

// 初始化中断服务例程
void isr_init();

#endif
