/* std */
#include <type.h>
/* lib */
#include <time.h>

void delay_once()
{
    uint32_t i;
    for (i=0; i<1<<16; i++);
}

void delay(uint32_t count)
{
    uint32_t i;
    for (i=0; i<count; i++)
    {
        delay_once();
    }
}
