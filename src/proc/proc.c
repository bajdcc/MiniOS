// std
#include <type.h>
#include <asm.h>
// x86
#include <pm.h>
#include <isr.h>
// libs
#include <string.h>
#include <print.h>
// mm
#include <mm.h>
// fs
#include <fs.h>
// proc
#include <proc.h>

extern void _isr_stub_ret();
extern void context_switch(struct context **old, struct context *new);


static uint32_t cur_pid = 0;
static struct proc *initproc = NULL;
static struct proc ptable[NPROC];
struct context *cpu_context;

struct proc *proc = NULL;

/* import pic_init(), only use in fork_ret */
extern void pic_init();
/* do not ask me why call pic_init here, irq didn't work after fork()
 * I have to reinit it
 * thx to fleuria
 */
void fork_ret() {
    static int fst = 1;

    if (fst == 1) {
        fst = 0;
    } else {
        pic_init(); 
    }

    return;
}

void print_proc(struct proc *pp) {
    uint32_t pa;

    vmm_get_mapping(pp->pgdir, USER_BASE, &pa);
}

static struct proc *proc_alloc() {
    struct proc *pp;
    char *tos;

    for (pp = &ptable[0]; pp < &ptable[NPROC]; pp++) {
        /* found */
        if (pp->state == P_UNUSED) {

            pp->state = P_USED;
            pp->pid = ++cur_pid;

            /* alloc kernstack */
            pp->kern_stack = (char *)pmm_alloc();
            tos = pp->kern_stack + PAGE_SIZE;

            tos -= sizeof(*pp->fm);
            pp->fm = (struct int_frame *)tos;

            tos -= 4;
            *(uint32_t *)tos = (uint32_t)_isr_stub_ret;

            tos -= sizeof(*pp->context);
            pp->context = (struct context *)tos;
            memset(pp->context, 0, sizeof(*pp->context));

            pp->context->eip = (uint32_t)fork_ret;

            return pp;
        }
    }
    return 0;
}

void proc_init() {
    struct proc *pp;
    extern char __init_start;
    extern char __init_end;

    cur_pid = 0;

    memset(ptable, 0, sizeof(struct proc) * NPROC);

    pp = proc_alloc();

    pp->pgdir = (pde_t *)pmm_alloc();

    kvm_init(pp->pgdir);
    vmm_map(pp->pgdir, (uint32_t)pp->kern_stack, (uint32_t)pp->kern_stack, PTE_P | PTE_R | PTE_K);

    uint32_t size = &__init_end - &__init_start;
    pp->size = PAGE_SIZE;

    uvm_init_fst(pp->pgdir, &__init_start, size);

    memset(pp->fm, 0, sizeof(*pp->fm));
    pp->fm->cs = (SEL_UCODE << 3) | 0x3;
    pp->fm->ds = (SEL_UDATA << 3) | 0x3;
    pp->fm->es = pp->fm->ds;
    pp->fm->fs = pp->fm->ds;
    pp->fm->gs = pp->fm->ds;
    pp->fm->ss = pp->fm->ds;
    pp->fm->eflags = FLAG_IF;
    pp->fm->user_esp = USER_BASE + PAGE_SIZE;
    pp->fm->eip = USER_BASE;

    strcpy(pp->name, "init");

    pp->cwd = p2i("/");
    pp->state = P_RUNABLE;

    initproc = pp;
}


void scheduler() {
    struct proc *pp;

    for (;;) {
        for (pp = &ptable[0]; pp < &ptable[NPROC]; pp++) {
            cli();
            if (pp->state != P_RUNABLE) {
                continue;
            }
            
            uvm_switch(pp);
            pp->state = P_RUNNING;

            proc = pp;
            context_switch(&cpu_context, pp->context);
            sti();
        }
    }
}

void sched() {
    if (proc->state == P_RUNNING) {
        proc->state = P_RUNABLE;
    }
    context_switch(&proc->context, cpu_context);
}

void sleep(void *chan) {
    /* go to sleep
     * WARNING: POSSIBLE DEAD LOCK
     */
    cli();
    proc->chan = chan;
    proc->state = P_SLEEPING;
    sti();

    sched();

    // wake up
    proc->chan = 0;

    pic_init();
}

void wakeup(void *chan) {
    struct proc *pp;

    for (pp = ptable; pp < &ptable[NPROC]; pp++) {
        if (pp->state == P_SLEEPING && pp->chan == chan) {
            pp->state = P_RUNABLE;
        }
    }
}

int wait() {
    uint8_t havekids, pid;
    struct proc* pp;

    for (;;) {
        havekids = 0;
        for (pp = ptable; pp <= &ptable[NPROC]; pp++) {
            if (pp->parent != proc) {
                continue;
            }

            havekids = 1;
            
            if (pp->state == P_ZOMBIE) {
                // can be clear
                pid = pp->pid;

                /* free mem */
                pmm_free((uint32_t)pp->kern_stack);
                pp->kern_stack = 0;
                uvm_free(pp->pgdir);

                pp->state = P_UNUSED;
                pp->pid = 0;
                pp->parent = 0;
                pp->name[0] = 0;
                pp->killed = 0;

                return pid;
            }
        }

        if (!havekids || proc->killed) {
            return -1;
        }

        // wait for chidren to exit
        sleep(proc);
    }
}

void exit() {
    struct proc *pp;
    int fd;
    
    for (fd = 0; fd < NOFILE; fd++) {
        if (proc->ofile[fd]) {
            fclose(proc->ofile[fd]);
            proc->ofile[fd] = 0;
        }
    }

    iput(proc->cwd);
    proc->cwd = 0;

    cli();
    
    for (pp = ptable; pp < &proc[NPROC]; pp++) {
        if (pp->parent == proc) {
            pp->parent = proc->parent;
            if (pp->state == P_ZOMBIE) {
                wakeup(proc->parent);
            }
        }
    }
    wakeup(proc->parent);
    
    proc->state = P_ZOMBIE;
    sti();

    sched();
    panic("exit: return form sched");
}

int kill(uint8_t pid) {
    struct proc *pp;

    for (pp = ptable; pp < &ptable[NPROC]; pp++) {
        if (pp->pid == pid) {
            pp->killed = 1;

            if (pp->state == P_SLEEPING) {
                pp->state = P_RUNABLE;
            }
            return 0;
        }
    }
    return -1;
}

int fork() {
    int i;
    struct proc *child;

    if ((child = proc_alloc()) == 0) {
        return -1;
    }

    child->pgdir = uvm_copy(proc->pgdir, proc->size);

    if (child->pgdir == 0) {
        panic("fork:");
        pmm_free((uint32_t)child->kern_stack);
        child->kern_stack = 0;
        child->state = P_UNUSED;
        return -1;
    }
    
    child->size = proc->size;
    child->parent = proc;
    *child->fm = *proc->fm; // return form same address

    child->fm->eax = 0;
    for (i = 0; i < NOFILE; i++) {
        if (proc->ofile[i]) {
            child->ofile[i] = fdup(proc->ofile[i]);
        }
    }

    child->cwd = idup(proc->cwd);
    strncpy(child->name, proc->name, sizeof(proc->name));

    child->state = P_RUNABLE;
    
    return child->pid;
}
