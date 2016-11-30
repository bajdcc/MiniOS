/* std */
#include <type.h>
#include <asm.h>
#include <debug.h>
/* x86 */
#include <pm.h>
#include <isr.h>
#include <timer.h>
#include <syscall.h>
/* vga */
#include <vga.h>
/* lib */
#include <print.h>
#include <time.h>
/* driver */
#include <kb.h>
#include <ide.h>
/* mm */
#include <mm.h>
/* proc */
#include <proc.h>
/* fs */
#include <fs.h>
// dev
#include <dev.h>

void print_ok(void)
{
    int i;
    for (i = 27; i <= 80; i++) {
        putchar(' ');
    }

    putchar('[');
    vga_setcolor(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    puts("OK");
    vga_setcolor(VGA_COLOR_LIGHTGREY, VGA_COLOR_BLACK);
    putchar(']');
}

void init(void)
{
    vga_init();

    puts("init gdt...           ");
    gdt_init();
    print_ok();

    puts("init idt...           ");
    idt_init();
    print_ok();
    
    puts("init isr...           ");
    isr_init();
    print_ok();

    puts("init timer...         ");
    timer_init(); 
    print_ok();

    puts("init pmm...           ");
    pmm_init();
    pmm_mem_info();
    print_ok();
    
    puts("init vmm...           ");
    vmm_init();
    print_ok();
    
    puts("init kb...            ");
    kb_init();
    print_ok();

    puts("init ide...           ");
    ide_init();
    print_ok();

    puts("init bcache...        ");
    bcache_init();
    print_ok();

    puts("init icache...        ");
    inode_init();
    print_ok();

    puts("init ftable...        ");
    file_init();
    print_ok();

    puts("init dev...           ");
    dev_init();
    print_ok();

    puts("init process...       ");
    proc_init();
    print_ok();
}

void test_time(void)
{
    int i;
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    for (i=0; i<100; i++)
    {
        delay(1);
        print("Time: %d\r", i);
    }
}

int os_main(void)
{
    init();
    
    delay(100);

    cls();

    vga_setcolor(VGA_COLOR_LIGHTBLUE, VGA_COLOR_BLACK);
    puts("Hello world!\n");
    puts("\n");
    
    test_time();
    
    scheduler();
    
    panic("Error");

LOOP:
    hlt();
    goto LOOP;

    return 0;
}
