#pragma once

#include <autoconf.h>
#include <microkit.h>
#include <stdint.h>

void gpt_init(void);
// uint64_t sys_now(void);
/* IRQ function routed through profiler*/
void timer_irq(microkit_channel ch);