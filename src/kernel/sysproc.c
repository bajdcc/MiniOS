#include <asm.h>
#include <print.h>
#include <proc.h>
#include <sysproc.h>

int sys_fork() {
    return fork();
}

int sys_exit() {
    exit();
    return 0;
}

int sys_exec() {
    printk("exec\n");
    return 0;
}

int sys_sleep() {
    wait_all();
    return 0;
}