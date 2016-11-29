/* debug.c
 * some short, assembly function.
 */
 
// std
#include <type.h>
#include <asm.h>
#include <vga.h>

// lib
#include <print.h>

void print_status(){
    uint16_t reg1, reg2, reg3, reg4;
    __asm__ __volatile__(     "mov %%cs, %0;"
            "mov %%ds, %1;"
            "mov %%es, %2;"
            "mov %%ss, %3;"
            : "=m"(reg1), "=m"(reg2), "=m"(reg3), "=m"(reg4));

    print("ring: %d\n", reg1 & 0x3);
    print("cs: %x\n", reg1);
    print("ds: %x\n", reg2);
    print("es: %x\n", reg3);
    print("ss: %x\n", reg4);
}

void panic(const char *msg){
    cli();

    vga_setcolor(VGA_COLOR_RED, VGA_COLOR_BLACK);

    print("***** KERNEL PANIC *****\n");

    print("Message: %s\n", msg);
    print("\n");
    print("Status:\n");
    print_status();

    for (;;) hlt();
}
