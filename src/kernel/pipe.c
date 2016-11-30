// std
#include <type.h>
// libs
#include <print.h>
// mm
#include <mm.h>
// fs
#include <fs.h>
// pipe
#include <pipe.h>
// proc
#include <proc.h>

int pipe_alloc(struct file **fr, struct file **fw) {
    struct pipe *p;

    p = 0; 
    *fr = *fw = 0;
    *fr = falloc();
    *fw = falloc();

    p = (struct pipe *)pmm_alloc();

    p->readopen = p->writeopen = 1;
    p->nread = p->nwrite = 0;

    (*fr)->type = F_PIPE;
    (*fr)->readable = 1;
    (*fr)->writeable = 0;
    (*fr)->pipe = p;
    (*fw)->type = F_PIPE;
    (*fw)->readable = 0;
    (*fw)->writeable = 1;
    (*fw)->pipe = p;

    return 0;
}

void pipe_close(struct pipe *p, int writeable) {
    if (writeable) {
        p->writeopen = 0;
        wakeup(&p->nread);
    } else {
        p->readopen= 0;
        wakeup(&p->nwrite);
    }

    if (p->readopen == 0 && p->writeopen == 0) {
        pmm_free((uint32_t)p);
    }
}

int pipe_write(struct pipe *p, char *addr, int n) {
    int i; 
    for (i = 0; i < n; i++) {
        while (p->nwrite == p->nread + PIPE_SIZE) {   // pipe is full
            if (p->readopen == 0 || proc->killed) {
                return -1;
            }
            wakeup(&p->nread);  // wakeup pipe_read
            sleep(&p->nwrite);
        }
        p->data[p->nwrite++ % PIPE_SIZE] = addr[i];
    }

    wakeup(&p->nread);
    return n;
}

int pipe_read(struct pipe *p, char *addr, int n) {
    int i;

    while (p->nread == p->nwrite && p->writeopen) {  // pile is empty and writeable
        if (proc->killed) {
            return -1;
        }
        sleep(&p->nread);
    }
    for (i = 0; i < n; i++) {
        if (p->nread == p->nwrite) {  // empty
            break;
        }

        addr[i] = p->data[p->nread++ % PIPE_SIZE];
    }
    wakeup(&p->nwrite);

    return i;
}
