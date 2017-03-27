#ifndef __IPC_H
#define __IPC_H

#include <proc.h>

#define TASK_TTY    0
#define TASK_SYS    1
#define TASK_WINCH  2
#define TASK_FS     3
#define TASK_MM     4

#define NTASK       16

#define TASK_SYS    1

#define TASK_ANY    -1
#define TASK_NONE   -2

// IPC消息模式
#define SEND        1
#define RECEIVE     2
#define BOTH        3   /* BOTH = (SEND | RECEIVE) */

#define SENDING     0x02    /* set when proc trying to send */
#define RECEIVING   0x04    /* set when proc trying to recv */
#define INTERRUPT   -10

// IPC消息种类
enum msg_type {
    /* 
     * 当收到中断时，发送硬中断消息
     */
    HARD_INT = 1,

    SYS_TICKS,
};

#define	RETVAL  u.m3.m3i1

void block(struct proc* p);
void unblock(struct proc* p);
int  msg_send(struct proc* current, int dest, MESSAGE* m);
int  msg_receive(struct proc* current, int src, MESSAGE* m);
int  deadlock(int src, int dest);
void reset_msg(MESSAGE* p);
int  send_recv(int function, int src_dest, MESSAGE* msg);

int  sys_sendrec(int function, int src_dest, MESSAGE* m, struct proc* p);

#endif