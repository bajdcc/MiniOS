#include <type.h>
#include <debug.h>
#include <asm.h>
#include <gdt.h>
#include <idt.h>
#include <pmm.h>
#include <vmm.h>
#include <uvm.h>
#include <isr.h>
#include <string.h>
#include <print.h>
#include <proc.h>

extern void _isr_stub_ret();
extern void _context_switch(struct context **old, struct context *new);

// 进程ID（每次递增，用于赋值给新进程）
static uint32_t cur_pid = 0;

// 进程PCB数组
static struct proc pcblist[NPROC];

// 当前进程的CPU上下文
struct context *cpu_context;

// 当前进程
struct proc *proc = NULL;

// 分叉返回
void fork_ret() {

}

// 进程切换
void context_switch(struct context **old, struct context* new) {
    _context_switch(old, new);
}

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
                _isr_stub_ret
                context
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

            // p = page_head + 页大小 - 中断现场结构大小 - 4
            p -= 4;

            // 中断返回地址 = fi - 4
            *(uint32_t *)p = (uint32_t)_isr_stub_ret;

            // p = page_head + 页大小 - 中断现场结构大小 - 4 - 上下文大小
            p -= sizeof(*pp->context);

            // 上下文地址 = page_head + 页大小 - 中断现场结构大小 - 4 - 上下文大小
            pp->context = (struct context *)p;
            
            // 当前指令执行地址为fork_ret
            pp->context->eip = (uint32_t)fork_ret;

            // 初始化优先级
            pp->priority = PRIOR_USER;

            // 重置时间片
            pp->ticks = 0;

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

    // PID计数清零
    cur_pid = 0;

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
    pp->fi->cs = (SEL_UCODE << 3) | 0x3; // 0x3 RPL=3
    pp->fi->ds = (SEL_UDATA << 3) | 0x3;
    pp->fi->es = pp->fi->ds;
    pp->fi->fs = pp->fi->ds;
    pp->fi->gs = pp->fi->ds;
    pp->fi->ss = pp->fi->ds;
    pp->fi->eflags = 0x200;
    pp->fi->user_esp = USER_BASE + PAGE_SIZE; // 堆栈顶部，因为是向下生长的
    pp->fi->eip = USER_BASE; // 返回地址为用户基址

    strcpy(pp->name, "init"); // 第一个进程名为init

    pp->state = P_RUNABLE; // 状态设置为可运行

    pp->priority = PRIOR_KERN; // 初始化优先级

    tss_set(SEL_KDATA << 3, (uint32_t)pp->stack + PAGE_SIZE); // 初始化内核任务状态段

    proc = pp;
}

// 进程切换
static void proc_switch(struct proc *pp) {
    pp->state = P_RUNNING;
    proc = pp; // 切换当前活动进程

    // 切换，将pp->context保存至cpu_context
    // 现场保存至cpu_context
    context_switch(&cpu_context, pp->context);
}

// 挑选可用进程
static struct proc *proc_pick() {
    struct proc *pp, *pp_ready;
    int greatest_ticks = 0;

    // 查找可用进程
    for (pp = &pcblist[0]; pp < &pcblist[NPROC]; pp++) {

        if (pp->state == P_RUNABLE) { // 进程是活动的

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

// 进程调度
void schedule() {
    struct proc *pp, *pp_ready;
    pp_ready = 0;

    while (!pp_ready) {
        
        pp_ready = proc_pick(); // 挑选进程

        if (!pp_ready) { // 所有进程时间片用完，重置
            for (pp = &pcblist[0]; pp < &pcblist[NPROC]; pp++) {
                if (pp->state == P_RUNABLE) {
                    pp->ticks = pp->priority;
                }
            }
        } else {
            pp = pp_ready;

            // 关中断
            cli();

            uvm_switch(pp);
            proc_switch(pp);

            // 开中断
            sti();
        }
    }
}

// 进程切换结束
void sched() {

    if (proc->state == P_RUNNING) {
        proc->state = P_RUNABLE;
    }

    // 切换返回，从cpu_context中恢复至proc->context
    // 从cpu_context中恢复现场
    context_switch(&proc->context, cpu_context);
}

// 进程休眠
void sleep(uint8_t pid) {
    struct proc *pp;

    cli();

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {
        // 若是状态为运行且pid匹配，则休眠
        if (pp->state == P_RUNNING && pp->pid == pid) {
            pp->state = P_SLEEPING;
        }
    }

    sti();

    sched(); // 切换到其他进程

    wakeup(pid); // 唤醒
}

// 唤醒进程
void wakeup(uint8_t pid) {
    struct proc *pp;

    cli();

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {
        // 若是状态为睡眠且pid匹配，则唤醒
        if (pp->state == P_SLEEPING && pp->pid == pid) {
            pp->state = P_RUNABLE;
        }
    }

    sti();
}

// 等待子进程
int wait() {
    uint8_t has_child;
    struct proc* pp;

    for (;;) {
        has_child = 0;

        for (pp = pcblist; pp <= &pcblist[NPROC]; pp++) {
            if (pp->parent != proc) {
                continue; // 没有子进程
            }

            has_child = 1;
        }

        if (!has_child || proc->state == P_ZOMBIE) {
            return -1; // 无需等待一个将被杀死的进程
        }

        if (proc->pid == pp->pid) { // 不等待自身
            return -1;
        }

        sleep(proc->pid); // 等待
    }
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

// 等待所有进程
void wait_all() {
    struct proc *pp;

    for (;;) {

        for (pp = pcblist; pp <= &pcblist[NPROC]; pp++) {
            
            if (pp->state == P_ZOMBIE) {

                cli();

                uvm_switch(pp);
                proc_destroy(pp);

                sti();

            } else if (pp->state != P_UNUSED) {

                sleep(proc->pid); // 休眠
            }
        }
    }
}

// 退出进程
void exit() {
    struct proc *pp;

    cli();
    
    for (pp = pcblist; pp < &proc[NPROC]; pp++) {
        if (pp->parent == proc) { // 找到子进程
            pp->parent = proc->parent; // 将子进程的父进程置为当前进程的父进程（当前进程skip）
            if (pp->state == P_ZOMBIE) { // 若子进程已死亡
                wakeup(proc->parent->pid); // 唤醒当前进程的父进程
            }
        }
    }

    wakeup(proc->parent->pid); // 唤醒当前进程的父进程
    
    proc->state = P_ZOMBIE; // 当前进程置为死亡状态

    sti();

    sched(); // 切换出当前进程

    panic("exit: return form sched");
}

// 杀死进程
int kill(uint8_t pid) {
    struct proc *pp;

    for (pp = pcblist; pp < &pcblist[NPROC]; pp++) {

        if (pp->pid == pid) { // 找到要杀死的进程
            pp->state = P_ZOMBIE; // 标记死亡状态
            return 0;
        }
    }

    return -1;
}

// 进程分叉
int fork() {
    struct proc *child; // 子进程

    if ((child = proc_alloc()) == 0) {
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
        panic("fork:");
        pmm_free((uint32_t)child->stack);
        child->stack = 0;
        child->state = P_UNUSED;
        return -1; // 创建进程失败
    }

    child->size = proc->size;   // 用户空间一致
    child->parent = proc;       // 设置父进程为当前进程
    *child->fi = *proc->fi;     // 拷贝中断现场

    child->fi->eax = 0;

    strncpy(child->name, proc->name, sizeof(proc->name)); // 拷贝进程名

    child->state = P_RUNABLE; // 置状态为可运行
    
    return child->pid; // 返回子进程PID
}

void irq_handler_clock(struct interrupt_frame *r) {
    
    if (!proc)
        return;

    if (proc->ticks > 0) {
        proc->ticks--;
        return;
    }
    
    schedule();
}