/* std */
#include <type.h>
#include <asm.h>
/* vga */
#include <vga.h>

int os_main(void)
{
    vga_init();

    cls();

    vga_setcolor(VGA_COLOR_LIGHTBLUE, VGA_COLOR_BLACK);
    puts("Hello world!");

LOOP:
    hlt();
    goto LOOP;

    return 0;
}
