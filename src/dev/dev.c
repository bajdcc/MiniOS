#include <dev.h>
#include <tty.h>
#include <print.h>

struct dev dtable[NDEV];

void dev_init() {
    tty_init(); 
    dtable[DEV_TTY].read = tty_read;
    dtable[DEV_TTY].write = tty_write;
}
