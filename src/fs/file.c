// std
#include <type.h>
// libs
#include <print.h>
#include <string.h>
// pipe
#include <pipe.h>
// fs
#include <fs.h>

static struct file ftable[NFILE];

void file_init() {
    memset(ftable, 0, sizeof(struct file)*NFILE);
}

struct file *falloc() {
    int i;

    for (i = 0; i < NFILE; i++) {
        if (ftable[i].ref == 0) {
            ftable[i].ref = 1;
            return &ftable[i];
        }
    }
    return 0;
}

struct file *fdup(struct file *fp) {
    fp->ref++;
    return fp;
}

void fclose(struct file *fp) {
    fp->ref--;
    if (fp->ref > 0) {
        return;
    }

    if (fp->type == F_PIPE) {
        pipe_close(fp->pipe, fp->writeable);
    }
    if (fp->type == F_INODE) {
        iput(fp->ip);
    }

    fp->type = F_NONE;
    fp->ref = 0;
}

int fstat(struct file *fp, struct stat *st) {

    if (fp->type == F_INODE) {
        ilock(fp->ip);
        istat(fp->ip, st);
        iunlock(fp->ip);

        return 0;
    }
    return -1;
}

int fread(struct file *fp, char *addr, uint32_t n) {
    int len = 0;

    if (!fp->readable) {
        return -1;
    }

    if (fp->type == F_PIPE) {
        return pipe_read(fp->pipe, addr, n);
    }

    if (fp->type == F_INODE) {
        ilock(fp->ip);
        if ((len = iread(fp->ip, addr, fp->off, n)) > 0) {
            fp->off += len;
        }
        iunlock(fp->ip);
    }

    return len;
}

int fwrite(struct file *fp, char *addr, uint32_t n) {

    if (!fp->writeable){
        return -1;
    }

    if (fp->type == F_PIPE) {
        return pipe_write(fp->pipe, addr, n);
    }

    if (fp->type == F_INODE) {
        ilock(fp->ip);
        iwrite(fp->ip, addr, fp->off, n);
        fp->off += n;
        iunlock(fp->ip);

        return n;
    }

    panic("fwrite: wrong type");
    return -1;
}
