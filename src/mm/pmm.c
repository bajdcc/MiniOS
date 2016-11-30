/* pmm.h
 * This file is modified form hurlex 
 * 内存物理页框分配，用栈管理从0x100000开始的所有有效物理地址
 * 同时提供了利用 int 0xe820 获取内存布局的函数
 * （该中断的执行在 boot/bootsector.asm:get_mem_map 中）
 * 内核的布局通过在ldscript中定位的变量获取
 */
 
// std
#include <type.h>
// libs
#include <string.h>
#include <print.h>
// mm
#include <mm.h>

/* these symbol's addr were remapped in ld script: script/link.ld
 * NB: only a symbol, not a variable
 * NB: 求两个变量地址的偏移的时候, 得到的是偏移是以该变量的步长为单位的(sizeof)
 * 因此这里只能用 uint8_t
 * ref: http://wiki.osdev.org/Using_Linker_Script_Values
 */
extern uint8_t kernstart;
extern uint8_t code;
extern uint8_t data;
extern uint8_t bss;
extern uint8_t kernend;

static uint32_t pmm_stack[PAGE_MAX_SIZE + 1];
static uint32_t pmm_stack_top = 0;     // top of stack
static uint32_t pmm_count = 0;
static uint32_t mem_size = 0;


uint32_t pmm_get_mem_sz() {
    return mem_size;
}

void pmm_mem_info() {
    /* 0x400 和 0x500 中的数据在bootsector.asm 中已经写入 */
    uint32_t MCR_count = *(uint32_t *)0x400;
    struct ARD_entry_s *ARD_entry = (struct ARD_entry_s *)0x500;
    uint32_t memsize = 0;
    uint32_t i = 0;

    /* do not care about high bit, we assume that the size of
     * memory always less than 512M */
    for (i = 0; i < MCR_count; i++) {
        if (ARD_entry[i].type == ADDR_RANGE_MEMORY
                && ARD_entry[i].base_addr_low + ARD_entry[i].len_low > memsize) {
            memsize = ARD_entry[i].base_addr_low + ARD_entry[i].len_low;
        }
    }
}

void pmm_init() {
    uint32_t MCR_count = *(uint32_t *)0x400;
    struct ARD_entry_s *ARD_entry = (struct ARD_entry_s *)0x500;
    uint32_t i = 0;

    for (i = 0; i < MCR_count; i++) {
        /* reach available memory */
        /* 如果该内存段有效且基址是0x100000, 将其加入栈中 */
        /* 从 kernend 之后再开始分配 */
       if (ARD_entry[i].type == ADDR_RANGE_MEMORY && ARD_entry[i].base_addr_low == 0x100000) {

           uint32_t addr = ((uint32_t)&kernend + PMM_PAGE_SIZE) & 0xfffff000;
           uint32_t limit = ARD_entry[i].base_addr_low + ARD_entry[i].len_low;

           mem_size = limit;

           while (addr < limit && addr <= PMM_MAX_SIZE) {
               //pmm_free(addr);
               /* 使用 pmm_free 会引入大量的 log */
               pmm_stack[pmm_stack_top++] = addr;   
               addr += PMM_PAGE_SIZE;
               pmm_count++;
           }
       }
    }
}

uint32_t pmm_alloc() {
    uint32_t addr = pmm_stack[--pmm_stack_top];

    memset((void *)addr, 0, PAGE_SIZE);
    return addr;
}

void pmm_free(uint32_t addr) {
    pmm_stack[pmm_stack_top++] = addr;
    memset((void *)addr, 1, PAGE_SIZE);
}
