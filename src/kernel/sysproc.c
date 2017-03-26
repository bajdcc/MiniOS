#include <asm.h>
#include <print.h>
#include <proc.h>
#include <syscall.h>
#include <sysproc.h>

int sys_fork() {
    return fork();
}

int sys_exit() {
    exit();
    return 0;
}

int sys_exec() {
    printk("exec: pid=%d\n", proc->pid);
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