/* vmm.h
 * 虚拟内存分配
 */

#include <type.h>
#include <string.h>
#include <pmm.h>
#include <vmm.h>

extern uint8_t kernstart;
extern uint8_t kernend;

/* 内核页表 = PTE_SIZE*PAGE_SIZE */
pde_t pgd_kern[PTE_SIZE] __attribute__((aligned(PAGE_SIZE)));
/* 内核页表内容 = PTE_COUNT*PTE_SIZE*PAGE_SIZE */
pte_t pte_kern[PTE_COUNT][PTE_SIZE] __attribute__((aligned(PAGE_SIZE)));

/* 修改CR3寄存器 */
inline void vmm_switch(uint32_t pde) {
    __asm__ volatile ("mov %0, %%cr3": :"r"(pde));
}

/* 刷新TLB */
// http://blog.csdn.net/cinmyheart/article/details/39994769
static inline void vmm_reflush(uint32_t va) {
    __asm__ volatile ("invlpg (%0)"::"a"(va));
}

// 允许分页
// http://blog.csdn.net/whatday/article/details/24851197
static inline void vmm_enable() {
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r" (cr0));
	cr0 |= CR0_PG;
	__asm__ volatile ("mov %0, %%cr0" : : "r" (cr0));
}

void vmm_init() {
    uint32_t i;

    // map 4G memory, physcial address = virtual address
    for (i = 0; i < PTE_SIZE; i++) {
        pgd_kern[i] = (uint32_t)pte_kern[i] | PTE_P | PTE_R | PTE_K;
    }
    
    uint32_t *pte = (uint32_t *)pte_kern;
    for (i = 1; i < PTE_COUNT*PTE_SIZE; i++) {
        pte[i] = (i << 12) | PTE_P | PTE_R | PTE_K; // i是页表号
    }

    vmm_switch((uint32_t)pgd_kern);
    vmm_enable();
}

// 虚页映射
// va = 虚拟地址  pa = 物理地址
void vmm_map(pde_t *pgdir, uint32_t va, uint32_t pa, uint32_t flags) {
    uint32_t pde_idx = PDE_INDEX(va); // 页目录号
    uint32_t pte_idx = PTE_INDEX(va); // 页表号

    pte_t *pte = (pte_t *)(pgdir[pde_idx] & PAGE_MASK); // 页表

    if (!pte) { // 缺页
        if (va >= USER_BASE) { // 若是用户地址则转换
            pte = (pte_t *)pmm_alloc(); // 申请物理页框，用作新页表
            pgdir[pde_idx] = (uint32_t)pte | PTE_P | flags; // 设置页表
            pte[pte_idx] = (pa & PAGE_MASK) | PTE_P | flags; // 设置页表项
        } else { // 内核地址不转换
            pte = (pte_t *)(pgd_kern[pde_idx] & PAGE_MASK); // 取得内核页表
            pgdir[pde_idx] = (uint32_t)pte | PTE_P | flags; // 设置页表
        }
    } else { // pte存在
        pte[pte_idx] = (pa & PAGE_MASK) | PTE_P | flags; // 设置页表项
    }
}

// 释放虚页
void vmm_unmap(pde_t *pde, uint32_t va) {
    uint32_t pde_idx = PDE_INDEX(va);
    uint32_t pte_idx = PTE_INDEX(va);

    pte_t *pte = (pte_t *)(pde[pde_idx] & PAGE_MASK);

    if (!pte) {
        return;
    }

    pte[pte_idx] = 0; // 清空页表项，此时有效位为零
    vmm_reflush(va); // 刷新TLB
}

// 是否已分页
int vmm_ismap(pde_t *pgdir, uint32_t va, uint32_t *pa) {
    uint32_t pde_idx = PDE_INDEX(va);
    uint32_t pte_idx = PTE_INDEX(va);

    pte_t *pte = (pte_t *)(pgdir[pde_idx] & PAGE_MASK);
    if (!pte) {
        return 0; // 页表不存在
    }
    if (pte[pte_idx] != 0 && (pte[pte_idx] & PTE_P) && pa) {
        *pa = pte[pte_idx] & PAGE_MASK; // 计算物理页面
        return 1; // 页面存在
    }
    return 0; // 页表项不存在
}