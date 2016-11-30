/* vmm.h
 * virtual memory management
 */

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


extern uint8_t kernstart;
extern uint8_t kernend;

extern uint8_t __init_start;
extern uint8_t __init_end;

pde_t pgd_kern[PDE_SIZE] __attribute__((aligned(PAGE_SIZE)));
/* page table entry of kernel */
static pte_t pte_kern[PTE_COUNT][PTE_SIZE] __attribute__((aligned(PAGE_SIZE)));

/* switch page table */
inline void vmm_switch_pgd(uint32_t pde) {
    __asm__ volatile ("mov %0, %%cr3": :"r"(pde));
}

/* reflush page table cache */
static inline void vmm_reflush(uint32_t va) {
    __asm__ volatile ("invlpg (%0)"::"a"(va));
}

static inline void vmm_enable() {
    uint32_t cr0;

    __asm__ volatile ("mov %%cr0, %0" : "=r" (cr0));
	cr0 |= CRO_PG;
	__asm__ volatile ("mov %0, %%cr0" : : "r" (cr0));
}

void vmm_init() {
    uint32_t i, j;
    /* register isr */

    // map 4G memory, physcial address = virtual address
    for (i = 0, j = 0; i < PTE_COUNT; i++, j++) {
        pgd_kern[i] = (uint32_t)pte_kern[j] | PTE_P | PTE_R | PTE_K;
    }
    
    uint32_t *pte = (uint32_t *)pte_kern;
    for (i = 1; i < PTE_COUNT*PTE_SIZE; i++) {
        pte[i] = (i << 12) | PTE_P | PTE_P | PTE_K;
    }

    vmm_switch_pgd((uint32_t)pgd_kern);

    vmm_enable();
}

void vmm_map(pde_t *pgdir, uint32_t va, uint32_t pa, uint32_t flags) {
    uint32_t pde_idx = PDE_INDEX(va);
    uint32_t pte_idx = PTE_INDEX(va);

    pte_t *pte = (pte_t *)(pgdir[pde_idx] & PAGE_MASK);

    if (!pte) {
        if (va >= USER_BASE) {
            pte = (pte_t *)pmm_alloc();
            memset(pte, 0, PAGE_SIZE);
            pgdir[pde_idx] = (uint32_t)pte | PTE_P | flags;
        } else {
            pte = (pte_t *)(pgd_kern[pde_idx] & PAGE_MASK);
            pgdir[pde_idx] = (uint32_t)pte | PTE_P | flags;
            return;
        }
    }

    pte[pte_idx] = (pa & PAGE_MASK) | PTE_P | flags;
}

void vmm_unmap(pde_t *pde, uint32_t va) {
    uint32_t pde_idx = PDE_INDEX(va);
    uint32_t pte_idx = PTE_INDEX(va);

    pte_t *pte = (pte_t *)(pde[pde_idx] & PAGE_MASK);

    if (!pte) {
        return;
    }
    /* unmap this poge */
    pte[pte_idx] = 0;

    vmm_reflush(va);
}

int vmm_get_mapping(pde_t *pgdir, uint32_t va, uint32_t *pa) {
    uint32_t pde_idx = PDE_INDEX(va);
    uint32_t pte_idx = PTE_INDEX(va);

    pte_t *pte = (pte_t *)(pgdir[pde_idx] & PAGE_MASK);
    if (!pte) {
        return 0;
    }
    if (pte[pte_idx] != 0 && (pte[pte_idx] & PTE_P) && pa) {
        *pa = pte[pte_idx] & PAGE_MASK;
        return 1;
    }
    return 0;
}

/* build a map of kernel space and unalloc memory for a process's page table
 * 这里简单地把从 0 到 0xc0000000 的所有内存（包含内核，未分配内存 和 不存在的内存）映射到页表中 */
void kvm_init(pde_t *pgdir) {
    uint32_t addr;

    for (addr = 0; addr < pmm_get_mem_sz(); addr += PAGE_SIZE) {
        vmm_map(pgdir, addr, addr, PTE_P | PTE_R | PTE_K);
    }
}

/* build a map of user space for a initproc's page table */
void uvm_init_fst(pde_t *pgdir, char *init, uint32_t size) {
    char *room;
    room = (char *)pmm_alloc();

    memset(room, 0, PAGE_SIZE);
    memcpy(room, init, size);

    vmm_map(pgdir, USER_BASE, (uint32_t)room, PTE_U | PTE_P | PTE_R);
}

void uvm_switch(struct proc *pp) {
    tss_set(SEL_KDATA << 3, (uint32_t)pp->kern_stack + PAGE_SIZE);
    vmm_switch_pgd((uint32_t)pp->pgdir);
}

int uvm_load(pte_t *pgdir, uint32_t addr, struct inode *ip, uint32_t off, uint32_t size) {
    uint32_t i, n, pa;

    for (i = 0; i < size; i += PAGE_SIZE) {
        vmm_get_mapping(pgdir, addr + i, &pa);

        if (size - i < PAGE_SIZE) {
            n = size - i;
        } else {
            n = PAGE_SIZE;
        }

        // pa = va now
        if (iread(ip, (char *)pa, off + i, n) != (int)n) {
            return -1;
        }
    }
    return 0;
}

/* free user space only */
void uvm_free(pte_t *pgdir) {
    uint32_t i;

    i = PDE_INDEX(USER_BASE);

    for (; i < PTE_COUNT; i++) {
        if (pgdir[i] & PTE_P) {
            pmm_free(pgdir[i] & PAGE_MASK);
        }
    }

    pmm_free((uint32_t)pgdir);
}

pde_t *uvm_copy(pte_t *pgdir, uint32_t size) {
    pde_t *pgd;
    uint32_t i, pa, mem;

    pgd = (pde_t *)pmm_alloc();
    memset(pgd, 0, PAGE_SIZE);

    kvm_init(pgd);

    for (i = 0; i < size; i += PAGE_SIZE) {
        vmm_get_mapping(pgdir, USER_BASE + i, &pa);
        mem = pmm_alloc();
        memcpy((void *)mem, (void *)pa, PAGE_SIZE);
        vmm_map(pgd, USER_BASE + i, mem, PTE_R | PTE_U | PTE_P);
    }
    return pgd;
}

int uvm_alloc(pte_t *pgdir, uint32_t old_sz, uint32_t new_sz) {
    uint32_t mem;
    uint32_t start;

    if (new_sz < old_sz) {
        return old_sz;
    }

    for (start = PAGE_ALIGN_UP(old_sz); start < new_sz; start += PAGE_SIZE) {
        mem = pmm_alloc(); 
        memset((void *)mem, 0, PAGE_SIZE);
        vmm_map(pgdir, start, mem, PTE_P | PTE_R | PTE_U);
    }

    return new_sz;
}
