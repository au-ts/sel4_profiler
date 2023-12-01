#pragma once

#include <autoconf.h>
#include <sel4cp.h>
#include <stdint.h>

void gpt_init(void);
// uint64_t sys_now(void);
/* IRQ function routed through profiler*/
void timer_irq(sel4cp_channel ch);