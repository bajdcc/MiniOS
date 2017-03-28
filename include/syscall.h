#ifndef __SYSCALL_H
#define __SYSCALL_H

#include <type.h>
#include <isr.h>

#define NSYSCALL    20

// 调用号是自己设置的（在以后的微内核架构中，系统调用转化为信号）
// 调用号： mov eax #n; int 0x80
#define SYS_FORK    1
#define SYS_EXIT    2
#define SYS_EXEC    3
#define SYS_SLEEP   4
#define SYS_WAIT    5
#define SYS_KILL    6
#define SYS_IPC     7 // IPC实现微内核

// 指定地址取整数
int fetchint(uint32_t addr, int *ip);

// 指定地址取字符串
int fetchstr(uint32_t addr, char **pp);

// 参数：取第n个整数
int argint(int n, int *ip);

// 参数：取第n个字符串，返回长度
int argstr(int n, char **pp);

// 参数：取第n个字符串地址
int argptr(int n, char **pp, int size);

// 初始化系统调用
void sys_init();

// 系统调用
void syscall();

// 用户调用
int call(int no);

#endif
