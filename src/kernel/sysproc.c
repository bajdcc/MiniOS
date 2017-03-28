#include <asm.h>
#include <print.h>
#include <proc.h>
#include <ipc.h>
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
    int i;

    if (argint(0, &i) < 0) {
        return -1;
    }
    printk("exec: pid=%d, arg=%d\n", proc->pid, i);
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

int sys_ipc() {
    int function, src_dest, caller;
    char *message;
    struct proc *pp;

    if (argint(0, &function) < 0) {
        return -1;
    }

    if (argint(1, &src_dest) < 0) {
        return -1;
    }

    if (argptr(2, &message, 0) < 0) {
        return -1;
    }

    if (argint(3, &caller) < 0) {
        return -1;
    }

    pp = npid(caller);
    
    return sys_sendrec(function, src_dest, (MESSAGE *)message, pp);
}

// ---------------------------

static void halt() {
    while (1);
}

static void delay() {
    int i;

    for (i = 0; i < 0x100000; i++);
}

void user_main() {

    int i;

    if ((i = call(SYS_FORK)) != 0) {
        delay();
        sys_tasks0();
        delay();
        call(SYS_WAIT);
    } else {
        delay();
        delay();
        call1(SYS_EXEC, i);
        delay();
        i = sys_ticks();
        delay();
        call1(SYS_EXEC, i);
        delay();
        call(SYS_EXIT);
    }
    halt();
}

void sys_tasks0() {
    extern uint32_t tick;

    MESSAGE msg;
    while (1) {
        send_recv(RECEIVE, TASK_ANY, &msg);
        int src = msg.source;
        switch (msg.type) {
        case SYS_TICKS:
            msg.RETVAL = tick;
            send_recv(SEND, src, &msg);
            break;
        default:
            assert(!"unknown msg type");
            break;
        }
    }
}

static int sys_ipc_call(int type) {
    MESSAGE msg;
    reset_msg(&msg);
    msg.type = type;
    send_recv(BOTH, TASK_SYS, &msg);
    return msg.RETVAL;
}

int sys_ticks() {
    return sys_ipc_call(SYS_TICKS);
}