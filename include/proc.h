#ifndef __PROC_H
#define __PROC_H

#include <type.h>
#include <vmm.h>

// 进程名称最大长度
#define PN_MAX_LEN 16

// 最大进程数量
#define NPROC      128

/* 进程状态 */
#define P_UNUSED    0x0 // 未使用
#define P_USED      0x1 // 使用中
#define P_RUNABLE   0x2 // 可运行
#define P_RUNNING   0x3 // 运行中
#define P_SLEEPING  0x4 // 休眠中
#define P_ZOMBIE    0x5 // 死亡

// 进程切换上下文，保存关键寄存器内容
struct context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;
};

// PCB process control block 进程控制块
// http://blog.csdn.net/wyzxg/article/details/4024340
struct proc {
    volatile uint8_t pid;           // 进程ID
    uint32_t size;                  // 用户空间大小
    uint8_t state;                  // 进程状态
    char name[PN_MAX_LEN];          // 进程名称
    struct interrupt_frame *fi;     // 中断现场
    struct context *context;        // 进程上下文
    pde_t *pgdir;                   // 虚页目录（一级页表）
    char *stack;                    // 进程内核堆栈
    struct proc *parent;            // 父进程
};

// 当前运行的进程
extern struct proc *proc;

// 当前CPU上下文
extern struct context *cpu_context;

// 初始化进程管理
void proc_init();

// 进程调度循环
void schedule();

// 进程切换结束
void sched();

// 进程分叉
int fork();

// 等待子进程
int wait();

// 等待所有进程
void wait_all();

// 进程休眠
void sleep(uint8_t pid);

// 进程唤醒
void wakeup(uint8_t pid);

// 杀死进程
int kill(uint8_t pid);

// 进程退出
void exit();

#endif
