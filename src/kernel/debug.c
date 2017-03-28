/* debug.c
 * some short, assembly function.
 */
 
#include <type.h>
#include <asm.h>
#include <vga.h>
#include <print.h>
#include <proc.h>

void print_status() {
    uint16_t reg1, reg2, reg3, reg4;
    __asm__ __volatile__(
            "mov %%cs, %0;"
            "mov %%ds, %1;"
            "mov %%es, %2;"
            "mov %%ss, %3;"
            : "=m"(reg1), "=m"(reg2), "=m"(reg3), "=m"(reg4));

    printk("ring: %d\n", reg1 & 0x3);
    printk("cs: %x\n", reg1);
    printk("ds: %x\n", reg2);
    printk("es: %x\n", reg3);
    printk("ss: %x\n", reg4);
}

void panic(const char *msg, int ring) {
    cli();

    vga_setcolor(VGA_COLOR_RED, VGA_COLOR_BLACK);

    printk("***** PANIC *****\n");

    if (proc) {
        printk("Current proc: `%s`(PID: %d)\n", proc->name, proc->pid);
    }

    printk("Ring: %d\n", ring);
    printk("Message: %s\n", msg);
    printk("\n");
    printk("Status:\n");
    print_status();

    for (;;) hlt();
}

// 断言失败
void assertion_failure(char *exp, char *file, char *base_file, int line)
{
    printk("[ Assert(%s) failed: file: %s, base_file: %s, line: %d ]\n",
           exp, file, base_file, line);

    /**
     * If assertion fails in a TASK, the system will halt before
     * printl() returns. If it happens in a USER PROC, printl() will
     * return like a common routine and arrive here. 
     * @see sys_printx()
     * 
     * We use a forever loop to prevent the proc from going on:
     */
    while (1);

    /* should never arrive here */
        __asm__ __volatile__("ud2");
}
