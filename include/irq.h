#ifndef __IRQ_H 
#define __IRQ_H 

#include <isr.h>

/* 参考: http://wiki.osdev.org/IRQ */

#define IRQ_TIMER   0
#define IRQ_KB      1
#define IRQ_IDE     14

// 中断处理
void irq_handler(struct interrupt_frame *r);
// 注册中断服务例程
void irq_install(uint8_t irq, void (*handler)(struct interrupt_frame *r));
// 卸载中断服务例程
void irq_uninstall(uint8_t irq);
// 初始化中断服务
void irq_init();

// 允许中断
void irq_enable(uint8_t irq);
// 屏蔽中断
void irq_disable(uint8_t irq);

// ---------------------------

// 初始化时钟
void irq_init_timer(void (*handler)(struct interrupt_frame *r));

#endif
