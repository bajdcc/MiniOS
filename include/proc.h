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

// 优先级就是时间片分配
#define PRIOR_SYST  0x20 // 系统服务优先级
#define PRIOR_USER  0x10 // 用户进程优先级

/**
 * MESSAGE 结构 / 借鉴自 MINIX
 */
struct msg1 {
    int m1i1;
    int m1i2;
    int m1i3;
    int m1i4;
};
struct msg2 {
    void* m2p1;
    void* m2p2;
    void* m2p3;
    void* m2p4;
};
struct msg3 {
    int	m3i1;
    int	m3i2;
    int	m3i3;
    int	m3i4;
    u64	m3l1;
    u64	m3l2;
    void* m3p1;
    void* m3p2;
};
typedef struct {
    int source;
    int type;
    union {
        struct msg1 m1;
        struct msg2 m2;
        struct msg3 m3;
    } u;
} MESSAGE;

// PCB process control block 进程控制块
// http://blog.csdn.net/wyzxg/article/details/4024340
struct proc {
    /* KERNEL */
    struct interrupt_frame *fi;     // 中断现场
    volatile uint8_t pid;           // 进程ID
    uint32_t size;                  // 用户空间大小
    uint8_t state;                  // 进程状态
    char name[PN_MAX_LEN];          // 进程名称
    pde_t *pgdir;                   // 虚页目录（一级页表）
    char *stack;                    // 进程内核堆栈
    struct proc *parent;            // 父进程
    int8_t ticks;                   // 时间片
    int8_t priority;                // 优先级
    /* IPC */
    int p_flags;                    // 标识
    MESSAGE *p_msg;                 // 消息
    int p_recvfrom;                 // 接收消息的进程ID
    int p_sendto;                   // 发送消息的进程ID
    int has_int_msg;                // nonzero if an INTERRUPT occurred when the task is not ready to deal with it.
    struct proc *q_sending;         // queue of procs sending messages to this proc
    struct proc *next_sending;      // next proc in the sending queue (q_sending)
};

// 当前运行的进程
extern struct proc *proc;

// 记录中断重入次数
extern int32_t k_reenter;

// 时钟
extern uint32_t tick;

// 初始化进程管理
void proc_init();

// 进程调度循环
void schedule();

// 进程分叉
int fork();

// 等待子进程
int wait();

// 进程休眠
void sleep();

// 进程唤醒
void wakeup(uint8_t pid);

// 杀死进程
int kill(uint8_t pid);

// 进程退出
void exit();

// ---------------------------------

#define proc2pid(x) ((x) -> pid)

struct proc *nproc(int offset);
struct proc *npid(int pid);
int npoffset(int pid);

void* va2la(int pid, void* va);

#endif
