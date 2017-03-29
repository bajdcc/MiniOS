#ifndef __GDT_H 
#define __GDT_H 

#include <type.h>

/* 参考: http://wiki.osdev.org/GDT */

// 全局描述符表结构 http://www.cnblogs.com/hicjiajia/archive/2012/05/25/2518684.html
// base: 基址
// limit: 寻址最大范围 tells the maximum addressable unit
// flags: 标志位 见上面的AC_AC等
struct gdt_entry {
    uint16_t limit_low;       
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    unsigned limit_high: 4;
    unsigned flags: 4;
    uint8_t base_high;
} __attribute__((packed));

// GDTR
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define NGDT 256        // 全局描述符表大小 Global Descriptor Table

#define AC_AC 0x1       // 可访问 access
#define AC_RW 0x2       // [代码]可读；[数据]可写 readable for code selector & writeable for data selector
#define AC_DC 0x4       // 方向位 direction
#define AC_EX 0x8       // 可执行 executable, code segment
#define AC_RE 0x10      // 保留位 reserve
#define AC_PR 0x80      // 有效位 persent in memory

// 特权位： 01100000b
#define AC_DPL_KERN 0x00 // RING 0 kernel level
#define AC_DPL_SYST 0x20 // RING 1 systask level
#define AC_DPL_USER 0x60 // RING 3 user level

#define GDT_GR  0x8     // 页面粒度 page granularity, limit in 4k blocks
#define GDT_SZ  0x4     // 大小位 size bt, 32 bit protect mode

// gdt selector 选择子
#define SEL_KCODE   0x1 // 内核代码段
#define SEL_KDATA   0x2 // 内核数据段
#define SEL_UCODE   0x3 // 用户代码段
#define SEL_UDATA   0x4 // 用户数据段
#define SEL_SCODE   0x5 // 用户代码段
#define SEL_SDATA   0x6 // 用户数据段
#define SEL_TSS     0x7 // 任务状态段 task state segment http://wiki.osdev.org/TSS

// RPL 请求特权等级 request privilege level
#define RPL_KERN    0x0
#define RPL_SYST    0x1
#define RPL_USER    0x3

// CPL 当前特权等级 current privilege level
#define CPL_KERN    0x0
#define CPL_SYST    0x1
#define CPL_USER    0x3

// 初始化GDT
void gdt_init();

#endif
