#ifndef __UVM_H
#define __UVM_H

#include <type.h>
#include <vmm.h>
#include <proc.h>

// 内核空间初始化
void kvm_init(pde_t *pgdir);

// 用户空间初始化
void uvm_init(pde_t *pgdir, char *init, uint32_t size); 

// 申请用户空间
int uvm_alloc(pte_t *pgdir, uint32_t old_sz, uint32_t new_sz);

// 用户页表切换
void uvm_switch(struct proc *pp);

// 释放用户空间
void uvm_free(pte_t *pgdir);

// 拷贝用户空间
pde_t *uvm_copy(pte_t *pgdir, uint32_t size);

#endif