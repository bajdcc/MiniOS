#include <type.h>
#include <asm.h>
#include <vga.h>
#include <print.h>
#include <debug.h>

#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <irq.h>
#include <pmm.h>
#include <vmm.h>
#include <uvm.h>
#include <syscall.h>
#include <sysproc.h>
#include <proc.h>

extern void restart();

void print_ok(void)
{
    putchar('[');
    vga_setcolor(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    puts("OK");
    vga_setcolor(VGA_COLOR_LIGHTGREY, VGA_COLOR_BLACK);
    putchar(']');
}

void init(void)
{
    vga_init();

    print_ok();
    puts(" init vga...\n");

    gdt_init();
    print_ok();
    puts(" init gdt...\n");

    idt_init();
    print_ok();
    puts(" init idt...\n");

    isr_init();
    print_ok();
    puts(" init isr...\n");

    pmm_init();
    print_ok();
    puts(" init pmm...");

    printk(" size: 0x%x\n", pmm_size());

    vmm_init();
    print_ok();
    puts(" init vmm...\n");

    sys_init();
    print_ok();
    puts(" init syscall...\n");

    proc_init();
    print_ok();
    puts(" init proc...\n");
}

int os_main(void)
{
    init();

    vga_setcolor(VGA_COLOR_LIGHTBLUE, VGA_COLOR_BLACK);

    puts("\n");
    puts("Hello world!  --- OS by bajdcc \n");
    puts("\n");

    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    irq_init_timer(irq_handler_clock);

    uvm_switch(proc);
    restart(); // 完成中断的后半部分，从而进入init进程

    while (1); // handle interrupt

    return 0;
}
