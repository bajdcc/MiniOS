#include <asm.h>
#include <print.h>
#include <proc.h>
#include <syscall.h>
#include <sysproc.h>

int sys_fork() {
    return fork();
}

int sys_exit() {
    //int i;
    //i = proc->pid;
    exit();
    //printk("proc#%d exit\n", i);
    return 0;
}

int sys_exec() {
    //printk("exec: pid=%d\n", proc->pid);
    return 0;
}

int sys_sleep() {
    sleep();
    return 0;
}

int sys_wait() {
    return wait();
}

int sys_kill() {
    int pid;

    if (argint(0, &pid) < 0) {
        return -1;
    }
    
    return kill(pid);
}

static void halt() {
    while (1);
}

static int delay() {
    volatile int i;

    for (i = 0; i < 0x1000000; i++);
    return i;
}

void user_main() {

    int i, j;

    i = call(SYS_FORK);

    if (i != 0) {
        printk("proc#%d wait %d\n", proc2pid(proc), i);
        call(SYS_WAIT);
        printk("proc#%d halt...\n", proc2pid(proc));
        halt();
    } else {
        for (j = 0; j < 20; j++) {
            printk("proc#%d tick %d\n", proc2pid(proc), j);
            delay();
        }
        printk("proc#%d exit\n", proc2pid(proc));
        call(SYS_EXIT);
    }
}
