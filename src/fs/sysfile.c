// std
#include <type.h>
#include <syscall.h>
#include <pipe.h>
// lib
#include <print.h>
#include <string.h>
// proc
#include <proc.h>
// fs
#include <fs.h>

/* get syscall param as file descrpitor */
static int argfd(int n, int *pfd, struct file **pf) {
    int fd;
    struct file *f;

    if (argint(n, &fd) < 0) {
        goto bad;
    }
    if (fd < 0 || fd >= NOFILE || (f = proc->ofile[fd]) == 0) {
        goto bad;
    }
    if (pfd) {
        *pfd = fd;
    }
    if (pf) {
        *pf = f;
    }

    return 0;
bad:
    return -1;
}

/* alloc a file descrpitor for given file */
static int fdalloc(struct file *f) {
    int fd;
    
    for (fd = 0; fd < NOFILE; fd++) {
        if (proc->ofile[fd] == 0) {
            proc->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

int sys_dup() {
    struct file *f;
    int fd;

    if (argfd(0, 0, &f) < 0) {
        return -1;
    }
    if ((fd = fdalloc(f)) < 0) {
        return -1;
    }
    fdup(f);

    return fd;
}

int sys_read() {
    struct file *f;
    int n;
    char *p;

    if (argfd(0, 0, &f) < 0  || argint(2, &n) < 0 || argptr(1, &p, n) < 0) {
        return -1;
    }

    return fread(f, p, n);
}

int sys_write() {
    struct file *f;
    int n;
    char *p;

    if (argfd(0, 0, &f) < 0  || argint(2, &n) < 0 || argptr(1, &p, n) < 0) {
        return -1;
    }
    return fwrite(f, p, n);
}

int sys_close() {
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0) {
        return -1;
    }
    proc->ofile[fd] = 0;
    fclose(f);

    return 0;
}

int sys_fstat() {
    struct file *f;
    struct stat *st;

    if (argfd(0, 0, &f) < 0 || argptr(1, (void *)&st, sizeof(st)) < 0) {
        return -1;
    }
    return fstat(f, st);
}

/* int _link(char *old, char *new); */
int sys_link() {
    char name[NAME_LEN], *new, *old;
    struct inode *dp, *ip;

    if (argstr(0, &old) < 0 || argstr(1, &new) < 0) {
        return -1;
    }
    if ((ip = p2i(old)) == 0) {
        return -1;
    }

    ilock(ip);

    if (S_ISDIR(ip->mode)) {     // don't create link for directory
        iunlockput(ip);
        return -1;
    }
    
    ip->nlinks++;
    iupdate(ip);

    if ((dp = p2ip(new, name)) == 0) {
        goto bad;
    }
    ilock(dp);
    /* don't worry about dev number */
    if (dir_link(dp, name, ip) < 0) {
        iunlockput(dp);
        goto bad;
    }
    iunlockput(dp);
    iunlockput(ip);

    return 0;

bad:
    ip->nlinks--;
    iupdate(ip);
    iunlockput(ip);

    return -1;
}

/* int _unlink(char *path) */
int sys_unlink() {
    uint32_t off;
    char name[NAME_LEN], *path;
    struct inode *ip, *dip;
    struct dir_entry de;

    if (argstr(0, &path) < 0) {
        return -1;
    }

    if ((dip = p2ip(path, name)) == 0) {
        return -1;
    }

    ilock(dip);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        goto bad;
    }

    if ((ip = dir_lookup(dip, name, &off)) == 0) {
        goto bad;
    }

    ilock(ip);

    /* we do not link any directory, so directory's nlinks always be 2 (self and ".") */
    if (S_ISDIR(ip->mode) && !dir_isempty(ip)) {
        goto bad1;
    }

    memset(&de, 0, sizeof(de));
    if (iwrite(dip, (char *)&de, off, sizeof(de)) != sizeof(de)) {
        panic("sys_unlink: write fault");
    }

    /* empty directory */
    if (S_ISDIR(ip->mode)) {
        dip->nlinks--;   // for ip/..
        iupdate(dip);
    }
    iunlockput(dip);

    ip->nlinks--;
    iupdate(ip);
    iunlockput(ip);

    return 0;
bad1:
    iunlockput(ip);
bad:
    iunlockput(dip);

    return -1;
}

static char illch[] = {' ', '\t', '\n', '\b', '/', '?', '*', '&', '>', '<', '|', 0}; 
static int islegal(char *name) {
    int i, j;
    for (i = 0; i < strlen(name); i++) {
        for (j = 0; j < strlen(illch); j++) {
            if (name[i] == illch[j]) return 0;
        }
    }
    return 1;
}

static struct inode *create(char *path, uint16_t mode, int di) {
    uint32_t off;
    struct inode *ip, *dip;
    char name[NAME_LEN];

    if ((dip = p2ip(path, name)) == 0) {
        return 0;
    }

    if (!islegal(name)) {    // illegal filename
        iput(dip);
        return 0;
    }

    ilock(dip);

    if ((ip = dir_lookup(dip, name, &off)) != 0) {
        iunlockput(dip);
        ilock(ip);
        if (S_ISREG(mode) && S_ISDIR(ip->mode)) {
            return ip;
        }
        iunlockput(ip);
        return 0;
    }

    if ((ip = ialloc(mode)) == 0) {
        panic("create: ialloc failed");
    }

    ilock(ip);
    ip->mode = mode;
    ip->nlinks = 1;
    iupdate(ip);

    if (S_ISDIR(mode)) {
        dip->nlinks++;  // for ".."
        iupdate(dip);
        ip->nlinks++;   // for "."
        iupdate(ip);
        if (dir_link(ip, ".", ip) < 0 || dir_link(ip, "..", dip) < 0) {
            panic("create: dir_link: make . & ..");
        }
    } 
    
    if (S_ISCHR(mode)) {
        /* device index */
        ip->zone[0] = di;
        iupdate(ip);
    }

    if (dir_link(dip, name, ip) < 0) {
        panic("create: dir_link: create name"); 
    }

    iunlockput(dip);

    return ip;
}

int sys_open() {
    char *path;
    int fd, omode;
    struct file *f;
    struct inode *ip;

    if (argstr(0, &path) < 0 || argint(1, &omode) < 0) {
        return -1;
    }

    if (omode & O_CREATE) {
       ip = create(path, S_IFREG, 0);
       if (ip == 0) {
           return -1;
       }
    } else {
        if ((ip = p2i(path)) == 0) {
            return -1;
        }

        ilock(ip);
        if (S_ISDIR(ip->mode) && omode != O_RONLY) {
            iunlockput(ip);
            return -1;
        }
    }

    if ((f = falloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f) {
            fclose(f);
        }
        iunlockput(ip);

        return -1;
    }
    iunlock(ip);

    f->type = F_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WONLY);
    f->writeable = !(omode & O_RONLY);

    return fd;
}

int sys_mkdir() {
    char *path;
    struct inode *ip;

    if (argstr(0, &path) < 0) {
        return -1;
    }

    if ((ip = create(path, S_IFDIR, 0)) == 0) {
        return -1;
    }
    iunlockput(ip);

    return 0;
}

/* make up a device file */
int sys_mknod() {
    struct inode *ip;
    char *path;
    int len, di;

    if ((len = argstr(0, &path)) < 0 || argint(1, &di) < 0) {
        return -1;
    }

    if ((ip = create(path, S_IFCHR, di)) == 0) {
            return -1;
    }

    iunlockput(ip);
    return 0;
}

int sys_chdir() {
    char *path;
    struct inode *ip;

    if (argstr(0, &path) < 0) {
        return -1;
    }

    if ((ip = p2i(path)) == 0) {
        return -1;
    }

    ilock(ip);
    if (!S_ISDIR(ip->mode)) {
        iunlock(ip);
        return -1;
    }

    iunlock(ip);
    iput(proc->cwd);
    proc->cwd = ip;

    return 0;
}

int sys_pipe() {
    int *fd;
    struct file *rf, *wf;
    int fd0 = -1, fd1 = -1;
 
    if (argptr(0, (void *)&fd, 2*sizeof(*fd)) < 0) {
        return -1;
    }

    if (pipe_alloc(&rf, &wf) < 0) {
        return -1;
    }

    if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0) {
        if (fd >= 0) {
            proc->ofile[fd0] = 0;
        }
        /* fd1 must be -1 */
        fclose(rf);
        fclose(wf);
        return -1;
    }
    fd[0] = fd0;
    fd[1] = fd1;

    return 0;
}
