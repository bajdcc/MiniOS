/* irq.c 
 * This file is modified form Bram's Kernel Development Tutorial
 * Handle the Interrupt Requests(IRQs)h raised by hardware device 
 */
 
#include <type.h>
#include <asm.h>
#include <idt.h>
#include <isr.h>
#include <irq.h>

/* These are own ISRs that point to our special IRQ handler
*  instead of the regular 'fault_handler' function */
// 中断服务例程，默认是由fault_handler处理
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

/* This array is actually an array of function pointers. We use
*  this to handle custom IRQ handlers for a given IRQ */
// 16级中断，初始化函数指针为空
void *irq_routines[ISR_NIRQ] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

/* This installs a custom IRQ handler for the given IRQ */
// 设置IRQ
void irq_install(uint8_t irq, void (*handler)(struct interrupt_frame *r)) {
    irq_disable(irq);
    irq_routines[irq] = handler;
    irq_enable(irq);
}

/* This clears the handler for a given IRQ */
// 清空IRQ
void irq_uninstall(uint8_t irq) {
    irq_disable(irq);
    irq_routines[irq] = 0;
}

/* in short: map irq 0-15 to int 32-47 */
// 8259A中断管理芯片 http://blog.csdn.net/github_30220885/article/details/47139671

/* Chip IO port */
// Master PIC
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
// Slave PIC
#define PIC2_CMD    0xa0
#define PIC2_DATA   0xa1

// end of intrrupte
#define PIC_EOI     0x20

#define ICW1_ICW4       0x01    /* ICW4 (not) needed */
#define ICW1_SINGLE     0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08    /* Level triggered (edge) mode */
#define ICW1_INIT       0x10    /* Initialization - required! */
 
#define ICW4_8086       0x01    /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02    /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode/master */
#define ICW4_SFNM       0x10    /* Special fully nested (not) */

void irq_remap() {
    
    /* 参考：http://wiki.osdev.org/8259_PIC */

    outb(PIC1_CMD, ICW1_INIT + ICW1_ICW4);    // starts the initialization sequence (in cascade mode)
    outb(PIC2_CMD, ICW1_INIT + ICW1_ICW4);

    outb(PIC1_DATA, 0x20);                    // ICW2: Master PIC vector offset
    outb(PIC2_DATA, 0x28);                    // ICW2: Slave PIC vector offset

    outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
 
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
 
    outb(PIC1_DATA, 0xFF);                    // disable all irq in Mister PIC
    outb(PIC2_DATA, 0xFF);                    // disable all irq in Slave PIC
}

/* We first remap the interrupt controllers, and then we install
*  the appropriate ISRs to the correct entries in the IDT. This
*  is just like installing the exception handlers */
void irq_init() {
    irq_remap();

    idt_install(ISR_IRQ0 + 0, (uint32_t)irq0, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN); 
    idt_install(ISR_IRQ0 + 1, (uint32_t)irq1, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 2, (uint32_t)irq2, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 3, (uint32_t)irq3, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 4, (uint32_t)irq4, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 5, (uint32_t)irq5, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 6, (uint32_t)irq6, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 7, (uint32_t)irq7, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 8, (uint32_t)irq8, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 9, (uint32_t)irq9, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 10, (uint32_t)irq10, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 11, (uint32_t)irq11, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 12, (uint32_t)irq12, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 13, (uint32_t)irq13, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 14, (uint32_t)irq14, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
    idt_install(ISR_IRQ0 + 15, (uint32_t)irq15, SEL_KCODE << 3, GATE_INT, IDT_PR|IDT_DPL_KERN);
}

void irq_handler(struct interrupt_frame *r) {
    /* This is a blank function pointer */
    void (*handler)(struct interrupt_frame *r);

    /* Find out if we have a custom handler to run for this
    *  IRQ, and then finally, run it */
    handler = irq_routines[r->int_no - ISR_IRQ0];
    if (handler) {
        handler(r);
    }

    /* If the IDT entry that was invoked was greater than 40
    *  (meaning IRQ8 - 15), then we need to send an EOI to
    *  the slave controller */
    if (r->int_no >= 40) {
        outb(PIC2_CMD, PIC_EOI);
    }
    /* In either case, we need to send an EOI to the master
    *  interrupt controller too */
    outb(PIC1_CMD, PIC_EOI);
}

// 允许中断
void irq_enable(uint8_t irq) {
    if(irq < 8) {
        outb(PIC1_DATA, inb(PIC1_DATA) & ~(1 << irq));
    } else {
        outb(PIC2_DATA, inb(PIC2_DATA) & ~(1 << irq));
    }
}

// 屏蔽中断
void irq_disable(uint8_t irq) {
    if(irq < 8) {
        outb(PIC1_DATA, inb(PIC1_DATA) | (1 << irq));
    } else {
        outb(PIC2_DATA, inb(PIC2_DATA) | (1 << irq));
    }
}

// 时钟中断 初始化 8253 PIT

/* 8253/8254 PIT (Programmable Interval Timer) */
#define TIMER0         0x40 /* I/O port for timer channel 0 */
#define TIMER_MODE     0x43 /* I/O port for timer mode control */
#define RATE_GENERATOR 0x34 /* 00-11-010-0 :
                            * Counter0 - LSB then MSB - rate generator - binary
                            */
#define TIMER_FREQ     1193182L /* clock frequency for timer in PC and AT */
#define HZ             100  /* clock freq (software settable on IBM-PC) */

void irq_init_timer(void (*handler)(struct interrupt_frame *r)) {
    outb(TIMER_MODE, RATE_GENERATOR);
    outb(TIMER0, (uint8_t) (TIMER_FREQ/HZ) );
    outb(TIMER0, (uint8_t) ((TIMER_FREQ/HZ) >> 8));
    irq_install(IRQ_TIMER, handler);
}