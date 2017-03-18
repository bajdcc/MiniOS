#ifndef __FAULT_H
#define __FAULT_H

// 异常处理

#include <isr.h>

void fault_init();
void fault_handler(struct interrupt_frame *r);

#endif