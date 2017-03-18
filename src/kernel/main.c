#include <type.h>
#include <asm.h>
#include <vga.h>
#include <print.h>
#include <debug.h>

#include <gdt.h>
#include <idt.h>
#include <isr.h>
#include <pmm.h>

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
}

int os_main(void)
{
    init();

    vga_setcolor(VGA_COLOR_LIGHTBLUE, VGA_COLOR_BLACK);

    puts("\n");
    puts("Hello world!  --- OS by bajdcc \n");
    puts("\n");

LOOP:
    hlt();
    goto LOOP;

    return 0;
}
