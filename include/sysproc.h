#ifndef __SYSPROC_H
#define __SYSPROC_H

void irq_handler_clock(struct interrupt_frame *r);

extern int _fork();
extern int _exit();
extern int _exec();
extern int _sleep();
extern int _wait();
extern int _kill();

int sys_fork();
int sys_exit();
int sys_exec();
int sys_sleep();
int sys_wait();
int sys_kill();

#endif