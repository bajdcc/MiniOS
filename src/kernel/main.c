/* std */
#include <type.h>
#include <asm.h>
#include <debug.h>
/* vga */
#include <vga.h>
/* lib */
#include <print.h>
#include <time.h>

void test_time(void)
{
    int i;
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    for (i=0; i<1000; i++)
    {
        delay(1);
        print("Time: %d\r", i);
    }
}

int os_main(void)
{
    vga_init();
    
    delay(1000);

    cls();

    vga_setcolor(VGA_COLOR_LIGHTBLUE, VGA_COLOR_BLACK);
    puts("Hello world!\n");
    puts("\n");
    
    test_time();
    
    panic("Error");

LOOP:
    hlt();
    goto LOOP;

    return 0;
}
