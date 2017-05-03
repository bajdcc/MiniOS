#include <type.h>
#include <debug.h>
#include <asm.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <uvm.h>
#include <isr.h>
#include <ipc.h>
#include <string.h>
#include <print.h>
#include <proc.h>

#define PROC_IS_RUNABLE(pp) ((pp)->state == P_RUNABLE && (pp)->p_flags == 0)

// 中断重入计数
int32_t k_reenter = 0;

// 进程ID（每次递增，用于赋值给新进程）
static uint32_t cur_pid = 0;

// 时钟
uint32_t tick = 0;

// 进程PCB数组
static struct proc pcblist[NPROC];

// 当前进程
struct proc *proc = NULL;

// 开辟新进程
static struct proc *proc_alloc() {
    struct proc *pp;
    char *p;

    // 查找空闲进程
    for (pp = &pcblist[0]; pp < &pcblist[NPROC]; pp++) {

        if (pp->state == P_UNUSED) { // 找到未使用的进程

            /*
                ------------------------------ 4KB
                interrupt_frame
                ...
                stack
                ------------------------------ 0KB
            */

            pp->state = P_USED; // 占用它
            pp->pid = ++cur_pid; // 进程ID递增

            pp->stack = (char *)pmm_alloc(); // 申请物理页框

            // p指针指向页头 p = page_head
            p = pp->stack;

            // p = page_head + 页大小 - 中断现场结构大小
            p = p + PAGE_SIZE - sizeof(*pp->fi);

            // 进程的中断现场fi 位置 = page_head + 页大小 - 中断现场结构大小
            pp->fi = (struct interrupt_frame *)p;

            // 初始化优先级
            pp->priority = PRIOR_USER;

            // 重置时间片
            pp->ticks = 0;

            // init ipc data
            pp->p_flags = 0;
            pp->p_recvfrom = TASK_NONE;
            pp->p_sendto = TASK_NONE;
            pp->has_int_msg = 0;
            pp->q_sending = 0;
            pp->next_sending = 0;

            return pp;
        }
    }
    return 0; // 创建失败返回空
}

// 初始化进程管理
void proc_init() {
    struct proc *pp;
    extern char __init_start;
    extern char __init_end;

    // PCB数组清零
    memset(pcblist, 0, sizeof(struct proc) * NPROC);

    // 初始化第一个进程
    pp = proc_alloc();

    // 初始化一级页表
    pp->pgdir = (pde_t *)pmm_alloc();

    // 初始化内核堆栈
    kvm_init(pp->pgdir);

    // 映射内核堆栈
    vmm_map(pp->pgdir, (uint32_t)pp->stack, (uint32_t)pp->stack, PTE_P | PTE_R | PTE_K);

    // 得到内核大小
    uint32_t size = &__init_end - &__init_start;

    // 用户空间大小是4KB
    pp->size = PAGE_SIZE;

    // 将内核数据拷贝至映射用户空间
    uvm_init(pp->pgdir, &__init_start, size);

    // 拷贝中断现场，在执行完_isr_stub_ret恢复
    pp->fi->cs = (SEL_SCODE << 3) | RPL_SYST; // 0x1 RPL=1
    pp->fi->ds = (SEL_SDATA << 3) | RPL_SYST; // SYS TASK
    pp->fi->es = pp->fi->ds;
    pp->fi->fs = pp->fi->ds;
    pp->fi->gs = pp->fi->ds;
    pp->fi->ss = pp->fi->ds;
    pp->fi->eflags = 0x202;
    pp->fi->user_esp = USER_BASE + PAGE_SIZE; // 堆栈顶部，因为是向下生长的
    pp->fi->eip = USER_BASE; // 从0xc0000000开始执行

    strcpy(pp->name, "init"); // 第一个进程名为init

    pp->state = P_RUNABLE; // 状态设置为可运行

    pp->priority = PRIOR_SYST; // 初始化优先级

    proc = pp;
}

// 进程切换
static void proc_switch(struct proc *pp) {
    if (proc->state == P_RUNNING) {
        proc->state = P_RUNABLE;
    }
    pp->state = P_RUNABLE;
    proc = pp; // 切换当前活动进程
}

// 挑选可用进程
static struct proc *proc_pick() {
    struct proc *pp, *pp_ready;
    int greatest_ticks = 0;

    // 查找可用进程
    for (pp = &pcblist[0]; pp < &pcblist[NPROC]; pp++) {

        if (PROC_IS_RUNABLE(pp)) { // 进程是活动的，且不堵塞

            if (pp->ticks > greatest_ticks) {
                greatest_ticks = pp->ticks;
                pp_ready = pp;
            }
        }
    }

    if (!greatest_ticks)
        return 0;
    return pp_ready;
}

// 重置时间片
static void reset_time() {
    int i;
    struct proc *pp;
    i = 0;
    for (pp = &pcblist[0]; pp < &pcblist[NPROC]; pp++) {
        if (PROC_IS_RUNABLE(pp)) {
            i++;
            pp->ticks = pp->priority;
        }
    }
    if (i == 0) {
        assert(!"no runable process!");
    }
}

// 进程调度
void schedule() {
    struct proc *pp, *pp_ready;
    pp_ready = 0;

    while (!pp_ready) {
        
        pp_ready = proc_pick(); // 挑选进程

        if (!pp_ready) { // 所有进程时间片用完，重置
            reset_time();
        } else {
            pp = pp_ready;

            // 关中断
            cli();

            uvm_switch(pp); // 切换页表
            proc_switch(pp); // 切换进程

            // 开中断
            sti();

            return;
        }
    }
}

// 进程休眠
void sleep() {
    cli();

    if (proc->state == P_RUNNING) {
        proc->state = P_SLEEPING;
    }

    sti();
}

// 唤醒进程
void wakeup(uint8_t pid) {
    struct proc* pp;

    cli();

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {
        if (pp->pid == pid && pp->state == P_SLEEPING) {
            pp->state = P_RUNABLE;
            return;
        }
    }

    sti();
}

// 进程销毁
static void proc_destroy(struct proc *pp) {

    pmm_free((uint32_t)pp->stack); // 释放堆栈内存
    pp->stack = 0;
    uvm_free(pp->pgdir); // 释放页表

    pp->state = P_UNUSED;
    pp->pid = 0;
    pp->parent = 0;
    pp->name[0] = 0;
}

// 等待子进程
int wait() {
    uint8_t has_child, pid;
    struct proc* pp;

    cli();

    has_child = 0;

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {
        if (pp->parent != proc) {
            continue; // 没有子进程
        }

        has_child = 1;

        if (pp->state == P_ZOMBIE) {

            pid = pp->pid;
            proc_destroy(pp);

            sti();

            return pid; // 返回刚刚退出的子进程ID
        }
    }

    if (!has_child || proc->state == P_ZOMBIE) {
        sti();
        return -1; // 无需等待一个将被杀死的进程
    }

    sleep(); // 等待，这里不能阻塞，因为要防止中断重入

    sti();

    return -1;
}

// 退出进程
void exit() {
    struct proc *pp;

    cli();

    if (!proc->parent) { // init
        sti();
        return;
    }

    wakeup(proc->parent->pid); // 唤醒当前进程的父进程
    proc->state = P_ZOMBIE; // 标记为僵尸进程，等待父进程wait来销毁
    
    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {
        if (pp->parent == proc) { // 找到子进程
            pp->parent = proc->parent; // 将子进程的父进程置为当前进程的父进程（当前进程skip）
            if (pp->state == P_ZOMBIE) { // 若子进程为僵尸进程
                wakeup(proc->parent->pid); // 唤醒当前进程的父进程
            }
        }
    }

    sti();
}

// 杀死进程
int kill(uint8_t pid) {
    struct proc *pp;

    cli();

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {

        if (pp->pid == pid) { // 找到要杀死的进程
            pp->state = P_ZOMBIE; // 标记为僵尸进程
            sti();
            return 0;
        }
    }

    sti();

    return -1;
}

// 进程分叉
int fork() {
    struct proc *child; // 子进程

    cli();

    if ((child = proc_alloc()) == 0) {
        sti();
        return -1; // 创建进程失败
    }

    // 将当前进程的用户空间拷贝至子进程
    child->pgdir = uvm_copy(proc->pgdir, proc->size);

    // 映射堆栈
    /* 这句的缺少才是导致bug的罪魁祸首
     * 具体原因是：未映射内存导致无法访问，进而导致系统reset
     */
    vmm_map(child->pgdir, (uint32_t)child->stack, (uint32_t)child->stack, PTE_P | PTE_R | PTE_U);

    if (child->pgdir == 0) { // 拷贝失败
        panic("fork:", child->fi->cs & 0x3);
        pmm_free((uint32_t)child->stack);
        child->stack = 0;
        child->state = P_UNUSED;
        sti();
        return -1; // 创建进程失败
    }

    child->size = proc->size;   // 用户空间一致
    child->parent = proc;       // 设置父进程为当前进程
    *child->fi = *proc->fi;     // 拷贝中断现场

    child->fi->eax = 0;

    strncpy(child->name, proc->name, sizeof(proc->name)); // 拷贝进程名

    child->state = P_RUNABLE; // 置状态为可运行

    sti();
    
    return child->pid; // 返回子进程PID
}

void irq_handler_clock(struct interrupt_frame *r) {

    tick++;

    if (!proc)
        return;

    if (PROC_IS_RUNABLE(proc)) {
        if (proc->ticks > 0) {
            proc->ticks--;
            return;
        }

        if (k_reenter != 0) { // 防止重入
            return;
        }
    }
    
    schedule();
}

struct proc *nproc(int offset) {
    return &pcblist[offset];
}

struct proc *npid(int pid) {
    struct proc* pp;

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {
        if (pp->pid == pid) {
            return pp;
        }
    }
    return 0;
}

int npoffset(int pid) {
    int i;

    for (i = 0; i < NPROC; i++) {
        if (pcblist[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

void* va2la(int pid, void* va) {
    struct proc* pp;

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {
        if (pp->pid == pid) {

            uint32_t pa;
            
            if (vmm_ismap(pp->pgdir, (uint32_t)va, &pa)) {
                return (void*)((uint32_t)pa + OFFSET_INDEX((uint32_t)va));
            }

            assert(!"vmm: va not mapped!");

            return 0;
        }
    }

    return 0;
}
