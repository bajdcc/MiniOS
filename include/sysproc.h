#ifndef __SYSPROC_H
#define __SYSPROC_H

void irq_handler_clock(struct interrupt_frame *r);

int sys_fork();
int sys_exit();
int sys_exec();
int sys_sleep();
int sys_wait();
int sys_kill();

// -------------------------------

extern void user_main();

#endif