// std
#include <type.h>
#include <asm.h>
// libs
#include <print.h>
#include <string.h>
// fs
#include <fs.h>
// mm
#include <mm.h>
// proc
#include <proc.h>
#include <elf.h>

int exec(char *path, char **argv) {
    int i;
    char *s, *name;
    uint32_t sz, sp, off, argc, pa, ustack[3 + MAX_ARGC + 1];
    pde_t *pgdir, *old_pgdir;
    struct inode *ip;
    struct elf32hdr eh;
    struct proghdr ph;

    pgdir = 0;
    i = off = 0;

    pgdir = (pde_t *)pmm_alloc();
    kvm_init(pgdir);

    // exception handle pgdir
    //
    if ((ip = p2i(path)) == 0) {
        goto bad;
    }
    ilock(ip);

    // read elf header
    if (iread(ip, (char *)&eh, 0, sizeof(eh)) < (int)sizeof(eh)) {
        goto bad;
    }

    if (eh.magic != ELF_MAGIC) {
        goto bad;
    }

    // load program to memory
    sz = USER_BASE;
    for (i = 0, off = eh.phoff; i < eh.phnum; i++, off += sizeof(ph)) {
        if (iread(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph)) {
            goto bad;
        }
        if (ph.type != ELF_PROG_LOAD) {
            continue;
        }
        if (ph.memsz < ph.filesz) {
            goto bad;
        }
        if ((sz = uvm_alloc(pgdir, sz, ph.vaddr + ph.memsz)) == 0) {
            goto bad;
        }
        if (uvm_load(pgdir, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
            goto bad;
        }
    }

    iunlockput(ip);
    ip = 0;

    /* build user stack */
    sz = PAGE_ALIGN_UP(sz);
    if ((sz = uvm_alloc(pgdir, sz, sz + 2*PAGE_SIZE)) == 0) {
        goto bad;
    }

    /* leave a unaccessable page between kernel stack */
    if (vmm_get_mapping(pgdir, sz - 2*PAGE_SIZE, &pa) == 0) {  // sz is no mapped
        goto bad;
    }
    vmm_map(pgdir, sz - 2*PAGE_SIZE, pa, PTE_K | PTE_P | PTE_W);

    sp = sz;
    if (vmm_get_mapping(pgdir, sz - PAGE_SIZE, &pa) == 0) {  // sz is no mapped
        goto bad;
    }
    pa += PAGE_SIZE;

    for (argc = 0; argv[argc]; argc++) {
        if (argc > MAX_ARGC) {
            goto bad;
        }
        // "+1" leava room for '\0'  "&~3" align 4
        sp = (sp - (strlen(argv[argc]) + 1)) & ~3;    // sync with pa
        pa = (pa - (strlen(argv[argc]) + 1)) & ~3;    

        strcpy((char *)pa, argv[argc]);
        ustack[3+argc] = sp;  // argv[argc]
    }

    ustack[3+argc] = 0;

    ustack[0] = 0xffffffff;
    ustack[1] = argc;   // count of arguments
    ustack[2] = sp - (argc+1)*4;    // pointer of argv[0]

    sp -= (3 + argc + 1)*4;
    pa -= (3 + argc + 1)*4;
    memcpy((void *)pa, ustack, (3 + argc + 1)*4);   // combine

    for (name = s = path; *s; s++) {
        if (*s == '/') {
            name = s + 1;
        }
    }

    cli();
    strncpy(proc->name, name, sizeof(proc->name));

    old_pgdir = proc->pgdir;
    proc->pgdir = pgdir;
    proc->size = sz - USER_BASE;
    proc->fm->eip = eh.entry;
    proc->fm->user_esp = sp;
    uvm_switch(proc);

    uvm_free(old_pgdir);
    old_pgdir  = 0;
    old_pgdir ++;
    sti();

    return 0;

bad:
    if (pgdir) {
        uvm_free(pgdir);
    }
    if (ip) {
        iunlockput(ip);
    }
    return -1;
}

