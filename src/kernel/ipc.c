#include <ipc.h>
#include <debug.h>
#include <string.h>
#include <print.h>
#include <proc.h>

extern int sendrec(int function, int src_dest, MESSAGE* msg, int caller);

static void ipc_dump(int function, int src_dest, MESSAGE* msg, int caller, const char *direction) {
    //const char *str;
    /*switch (function) {
        case SEND:
            str = "[ipc] proc#%d %s send | %d --> %d(%s)\n";
            break;
        case RECEIVE:
            str = "[ipc] proc#%d %s recv | %d <-- %d(%s)\n";
            break;
        default:
            str = "[ipc] proc#%d %s ???? | %d <-- %d(%s)\n";
            break;
    }*/
    //printk(str, proc2pid(proc), direction, caller, src_dest, (src_dest == TASK_ANY) ? "task_any" : "task_normal");
}

int _sendrec(int function, int src_dest, MESSAGE* msg, int caller) {
    int ret;
    ipc_dump(function, src_dest, msg, caller, ">>");
    ret = sendrec(function, src_dest, msg, caller);
    ipc_dump(function, src_dest, msg, caller, "<<");
    return ret;
}

/*****************************************************************************
 *                                sys_sendrec
 *****************************************************************************/
/**
 * <Ring 0> The core routine of system call `sendrec()'.
 * 
 * @param function SEND or RECEIVE
 * @param src_dest To/From whom the message is transferred.
 * @param m        Ptr to the MESSAGE body.
 * @param p        The caller proc.
 * 
 * @return 0 if success.
 *****************************************************************************/
int sys_sendrec(int function, int src_dest, MESSAGE* m, struct proc* p)
{
    assert(k_reenter == 0);    /* make sure we are not in ring0 */
    assert((src_dest > 0) ||
           src_dest == TASK_ANY ||
           src_dest == INTERRUPT);

    int ret = 0;
    int caller = proc2pid(p);
    MESSAGE* mla = (MESSAGE*)va2la(caller, m); // 获取调用方消息
    mla->source = caller;

    assert(mla->source != src_dest); //确保不向自身发消息，防止死锁

    /**
     * Actually we have the third message type: BOTH. However, it is not
     * allowed to be passed to the kernel directly. Kernel doesn't know
     * it at all. It is transformed into a SEND followed by a RECEIVE
     * by `send_recv()'.
     */
    if (function == SEND) {
        ret = msg_send(p, src_dest, m); // 发送消息
    }
    else if (function == RECEIVE) {
        ret = msg_receive(p, src_dest, m); // 接收消息
    }
    else {
        printk("invalid function: "
              "%d (SEND:%d, RECEIVE:%d).", function, SEND, RECEIVE);
        assert(!"sys_sendrec failed");
    }

    return ret;
}

/*****************************************************************************
 *                                send_recv
 *****************************************************************************/
/**
 * <Ring 1~3> IPC syscall.
 *
 * It is an encapsulation of `sendrec',
 * invoking `sendrec' directly should be avoided
 *
 * @param function  SEND, RECEIVE or BOTH
 * @param src_dest  The caller's proc_nr
 * @param msg       Pointer to the MESSAGE struct
 * 
 * @return always 0.
 *****************************************************************************/
int send_recv(int function, int src_dest, MESSAGE* msg)
{
    int ret = 0, caller;

    caller = proc2pid(proc);

    if (function == RECEIVE)
        memset(msg, 0, sizeof(MESSAGE));

    switch (function) {
    case BOTH: // 先发送再接收
        ret = _sendrec(SEND, src_dest, msg, caller);
        if (ret == 0)
            ret = _sendrec(RECEIVE, src_dest, msg, caller);
        break;
    case SEND:
    case RECEIVE:
        ret = _sendrec(function, src_dest, msg, caller);
        break;
    default:
        assert((function == BOTH) ||
               (function == SEND) || (function == RECEIVE));
        break;
    }

    return ret;
}

/*****************************************************************************
 *                                reset_msg
 *****************************************************************************/
/**
 * <Ring 0~3> Clear up a MESSAGE by setting each byte to 0.
 * 
 * @param p  The message to be cleared.
 *****************************************************************************/
void reset_msg(MESSAGE* p)
{
    memset(p, 0, sizeof(MESSAGE));
}

/*****************************************************************************
 *                                block
 *****************************************************************************/
/**
 * <Ring 0> This routine is called after `p_flags' has been set (!= 0), it
 * calls `schedule()' to choose another proc as the `proc_ready'.
 *
 * @attention This routine does not change `p_flags'. Make sure the `p_flags'
 * of the proc to be blocked has been set properly.
 * 
 * @param p The proc to be blocked.
 *****************************************************************************/
void block(struct proc* p)
{
    int pid;
    pid = proc2pid(p);
    assert(p->p_flags);
    __asm__("int $0x20"); // 强制切换
    assert(pid != proc2pid(proc)); // 确保切换成功
}

/*****************************************************************************
 *                                unblock
 *****************************************************************************/
/**
 * <Ring 0> This is a dummy routine. It does nothing actually. When it is
 * called, the `p_flags' should have been cleared (== 0).
 * 
 * @param p The unblocked proc.
 *****************************************************************************/
void unblock(struct proc* p)
{
    assert(p->p_flags == 0);
}

/*****************************************************************************
 *                                deadlock
 *****************************************************************************/
/**
 * <Ring 0> Check whether it is safe to send a message from src to dest.
 * The routine will detect if the messaging graph contains a cycle. For
 * instance, if we have procs trying to send messages like this:
 * A -> B -> C -> A, then a deadlock occurs, because all of them will
 * wait forever. If no cycles detected, it is considered as safe.
 * 
 * @param src   Who wants to send message.
 * @param dest  To whom the message is sent.
 * 
 * @return Zero if success.
 *****************************************************************************/
int deadlock(int src, int dest)
{
    struct proc* p = npid(dest);
    while (1) {
        if (p->p_flags & SENDING) {
            if (p->p_sendto == src) {
                return 1;
            }
            p = npid(p->p_sendto);
        }
        else {
            break;
        }
    }
    return 0;
}

/*****************************************************************************
 *                                msg_send
 *****************************************************************************/
/**
 * <Ring 0> Send a message to the dest proc. If dest is blocked waiting for
 * the message, copy the message to it and unblock dest. Otherwise the caller
 * will be blocked and appended to the dest's sending queue.
 * 
 * @param current  The caller, the sender.
 * @param dest     To whom the message is sent.
 * @param m        The message.
 * 
 * @return Zero if success.
 *****************************************************************************/
int msg_send(struct proc* current, int dest, MESSAGE* m)
{
    struct proc* sender = current;
    struct proc* p_dest = npid(dest); /* proc dest */

    assert(proc2pid(sender) != proc2pid(p_dest));

    /* check for deadlock here */
    if (deadlock(proc2pid(sender), dest)) {
        printk("DEADLOCK! %d --> %d\n", sender->pid, p_dest->pid);
        assert(!"DEADLOCK");
    }

    if ((p_dest->p_flags & RECEIVING) && /* dest is waiting for the msg */
        (p_dest->p_recvfrom == proc2pid(sender) ||
         p_dest->p_recvfrom == TASK_ANY)) {
        assert(p_dest->p_msg);
        assert(m);

        memcpy(va2la(dest, p_dest->p_msg),
              va2la(proc2pid(sender), m),
              sizeof(MESSAGE));

        p_dest->p_msg = 0;
        p_dest->p_flags &= ~RECEIVING; /* dest has received the msg */
        p_dest->p_recvfrom = TASK_NONE;
        unblock(p_dest);

        assert(p_dest->p_flags == 0);
        assert(p_dest->p_msg == 0);
        assert(p_dest->p_recvfrom == TASK_NONE);
        assert(p_dest->p_sendto == TASK_NONE);
        assert(sender->p_flags == 0);
        assert(sender->p_msg == 0);
        assert(sender->p_recvfrom == TASK_NONE);
        assert(sender->p_sendto == TASK_NONE);
    }
    else { /* dest is not waiting for the msg */
        sender->p_flags |= SENDING;
        assert(sender->p_flags == SENDING);
        sender->p_sendto = dest;
        sender->p_msg = m;

        /* append to the sending queue */
        struct proc * p;
        if (p_dest->q_sending) {
            p = p_dest->q_sending;
            while (p->next_sending)
                p = p->next_sending;
            p->next_sending = sender;
        }
        else {
            p_dest->q_sending = sender;
        }
        sender->next_sending = 0;

        block(sender);

        assert(sender->p_flags == SENDING);
        assert(sender->p_msg != 0);
        assert(sender->p_recvfrom == TASK_NONE);
        assert(sender->p_sendto == dest);
    }

    return 0;
}


/*****************************************************************************
 *                                msg_receive
 *****************************************************************************/
/**
 * <Ring 0> Try to get a message from the src proc. If src is blocked sending
 * the message, copy the message from it and unblock src. Otherwise the caller
 * will be blocked.
 * 
 * @param current The caller, the proc who wanna receive.
 * @param src     From whom the message will be received.
 * @param m       The message ptr to accept the message.
 * 
 * @return  Zero if success.
 *****************************************************************************/
int msg_receive(struct proc* current, int src, MESSAGE* m)
{
    struct proc* p_who_wanna_recv = current;
    struct proc* p_from = 0; /* from which the message will be fetched */
    struct proc* prev = 0;
    int copyok = 0;

    assert(proc2pid(p_who_wanna_recv) != src);

    if ((p_who_wanna_recv->has_int_msg) &&
        ((src == TASK_ANY) || (src == INTERRUPT))) {
        /* There is an interrupt needs p_who_wanna_recv's handling and
         * p_who_wanna_recv is ready to handle it.
         */

        MESSAGE msg;
        reset_msg(&msg);
        msg.source = INTERRUPT;
        msg.type = HARD_INT;
        assert(m);
        memcpy(va2la(proc2pid(p_who_wanna_recv), m), &msg,
              sizeof(MESSAGE));

        p_who_wanna_recv->has_int_msg = 0;

        assert(p_who_wanna_recv->p_flags == 0);
        assert(p_who_wanna_recv->p_msg == 0);
        assert(p_who_wanna_recv->p_sendto == TASK_NONE);
        assert(p_who_wanna_recv->has_int_msg == 0);

        return 0;
    }


    /* Arrives here if no interrupt for p_who_wanna_recv. */
    if (src == TASK_ANY) {
        /* p_who_wanna_recv is ready to receive messages from
         * TASK_ANY proc, we'll check the sending queue and pick the
         * first proc in it.
         */
        if (p_who_wanna_recv->q_sending) {
            p_from = p_who_wanna_recv->q_sending;
            copyok = 1;

            assert(p_who_wanna_recv->p_flags == 0);
            assert(p_who_wanna_recv->p_msg == 0);
            assert(p_who_wanna_recv->p_recvfrom == TASK_NONE);
            assert(p_who_wanna_recv->p_sendto == TASK_NONE);
            assert(p_who_wanna_recv->q_sending != 0);
            assert(p_from->p_flags == SENDING);
            assert(p_from->p_msg != 0);
            assert(p_from->p_recvfrom == TASK_NONE);
            assert(p_from->p_sendto == proc2pid(p_who_wanna_recv));
        }
    }
    else {
        /* p_who_wanna_recv wants to receive a message from
         * a certain proc: src.
         */
        p_from = npid(src);

        if ((p_from->p_flags & SENDING) &&
            (p_from->p_sendto == proc2pid(p_who_wanna_recv))) {
            /* Perfect, src is sending a message to
             * p_who_wanna_recv.
             */
            copyok = 1;

            struct proc* p = p_who_wanna_recv->q_sending;
            assert(p); /* p_from must have been appended to the
                    * queue, so the queue must not be NULL
                    */
            while (p) {
                assert(p_from->p_flags & SENDING);
                if (proc2pid(p) == proc2pid(npid(src))) { /* if p is the one */
                    p_from = p;
                    break;
                }
                prev = p;
                p = p->next_sending;
            }

            assert(p_who_wanna_recv->p_flags == 0);
            assert(p_who_wanna_recv->p_msg == 0);
            assert(p_who_wanna_recv->p_recvfrom == TASK_NONE);
            assert(p_who_wanna_recv->p_sendto == TASK_NONE);
            assert(p_who_wanna_recv->q_sending != 0);
            assert(p_from->p_flags == SENDING);
            assert(p_from->p_msg != 0);
            assert(p_from->p_recvfrom == TASK_NONE);
            assert(p_from->p_sendto == proc2pid(p_who_wanna_recv));
        }
    }

    if (copyok) {
        /* It's determined from which proc the message will
         * be copied. Note that this proc must have been
         * waiting for this moment in the queue, so we should
         * remove it from the queue.
         */
        if (p_from == p_who_wanna_recv->q_sending) { /* the 1st one */
            assert(prev == 0);
            p_who_wanna_recv->q_sending = p_from->next_sending;
            p_from->next_sending = 0;
        }
        else {
            assert(prev);
            prev->next_sending = p_from->next_sending;
            p_from->next_sending = 0;
        }

        assert(m);
        assert(p_from->p_msg);
        /* copy the message */
        memcpy(va2la(proc2pid(p_who_wanna_recv), m),
              va2la(proc2pid(p_from), p_from->p_msg),
              sizeof(MESSAGE));

        p_from->p_msg = 0;
        p_from->p_sendto = TASK_NONE;
        p_from->p_flags &= ~SENDING;
        unblock(p_from);
    }
    else {  /* nobody's sending TASK_ANY msg */
        /* Set p_flags so that p_who_wanna_recv will not
         * be scheduled until it is unblocked.
         */
        p_who_wanna_recv->p_flags |= RECEIVING;

        p_who_wanna_recv->p_msg = m;

        if (src == TASK_ANY)
            p_who_wanna_recv->p_recvfrom = TASK_ANY;
        else
            p_who_wanna_recv->p_recvfrom = proc2pid(p_from);

        block(p_who_wanna_recv);

        assert(p_who_wanna_recv->p_flags == RECEIVING);
        assert(p_who_wanna_recv->p_msg != 0);
        assert(p_who_wanna_recv->p_recvfrom != TASK_NONE);
        assert(p_who_wanna_recv->p_sendto == TASK_NONE);
        assert(p_who_wanna_recv->has_int_msg == 0);
    }

    return 0;
}