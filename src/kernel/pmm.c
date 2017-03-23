/* pmm.h
 * 内存物理页分配
 */
 
#include <type.h>
#include <string.h>
#include <pmm.h>

extern uint8_t kernstart; // 内核开始地址
extern uint8_t code;
extern uint8_t data;
extern uint8_t bss;
extern uint8_t kernend;   // 内核结束地址

static uint32_t pmm_stack[PAGE_MAX_SIZE + 1]; // 可用内存表
static uint32_t pmm_stack_top = 0; // 栈顶
static uint32_t pmm_count = 0;
static uint32_t mem_size = 0;

void pmm_init() {
    // ARD_count和ARD在bootsect.asm中已计算完毕
    uint32_t ARD_count = *(uint32_t *)0x400;
    struct ard_entry *ARD = (struct ard_entry *)0x500;
    uint32_t i = 0;

    for (i = 0; i < ARD_count; i++) {

       if (ARD[i].type == ADDR_RANGE_MEMORY && // 如果该内存段有效
           ARD[i].base_addr_low == 0x100000) { // 且基址是0x100000

           uint32_t addr = ((uint32_t)&kernend + PMM_PAGE_SIZE) & 0xfffff000;
           uint32_t limit = ARD[i].base_addr_low + ARD[i].len_low;

           while (addr < limit && addr <= PMM_MAX_SIZE) {
               mem_size += PMM_PAGE_SIZE;
               pmm_stack[pmm_stack_top++] = addr; // 将空闲内存映射到pmm_stack中
               addr += PMM_PAGE_SIZE;
               pmm_count++;
           }
       }
    }
}

uint32_t pmm_size() {
    return mem_size;
}

uint32_t pmm_alloc() {
    uint32_t addr = pmm_stack[--pmm_stack_top];
    memset((void *)addr, 0, PMM_PAGE_SIZE); // 申请的内容清零
    return addr;
}

void pmm_free(uint32_t addr) {
    pmm_stack[pmm_stack_top++] = addr;
    memset((void *)addr, 1, PMM_PAGE_SIZE);
}
