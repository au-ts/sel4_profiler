#include <stdbool.h>
#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "printf.h"

#define ARMV8_PMEVTYPER_EVTCOUNT_MASK 0x3ff
#define ARMV8_PMCNTENSET_EL0_ENABLE (1<<31) /* *< Enable Perf count reg */
uintptr_t uart_base;

static void
enable_cycle_counter_el0()
{
	uint64_t init_cnt = 0xffffffffffffffff - 0x1999999999999999;
    	// uint64_t init_cnt = 0xffffffffffffffff - ;
    // uint64_t init_cnt = 0;

    uint64_t val;
	// /* Disable cycle counter overflow interrupt */
	// asm volatile("msr pmintenclr_el0, %0" : : "r" ((uint64_t)(0 << 31)));
	// /* Enable cycle counter */
	asm volatile("msr pmcntenset_el0, %0" :: "r" BIT(31));
    /* Enable user-mode access to cycle counters. */
	// asm volatile("msr pmuserenr_el0, %0" : : "r"(BIT(0) | BIT(2)));
    printf_("Finished enabling the clock counter\n");
	/* Clear cycle counter and start */

	asm volatile("mrs %0, pmcr_el0" : "=r" (val));
	val |= (BIT(0) | BIT(2));
    // serial_server_printf("Cleared the counter, and starting it.\n");
	asm volatile("isb" : :);
    asm volatile("msr pmcr_el0, %0" : : "r" (val));
	val = BIT(27);
	asm volatile("msr pmccfiltr_el0, %0" : : "r" (val));
    asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));

    uint32_t r;
    asm volatile("mrs %0, pmccntr_el0" : "=r" (r));
    printf_("This is the current cycle counter: %d\n", r);
}

static void
init_pmu_event() {
    printf_("Initialising a PMU event\n");
    uint32_t evtCount = 0x00;
    uint32_t evtCount2 = 0x04;
    /* Setup PMU counter to record specific event */
    /* evtCount is the event id */
    evtCount &= ARMV8_PMEVTYPER_EVTCOUNT_MASK;
    evtCount2 &= ARMV8_PMEVTYPER_EVTCOUNT_MASK;
    asm volatile("isb");
    /* Just use counter 0 here */
    asm volatile("msr pmevtyper0_el0, %0" : : "r" (evtCount));
    asm volatile("isb");
    asm volatile("msr pmevtyper1_el0, %0" : : "r" (evtCount2));
    /*   Performance Monitors Count Enable Set register bit 30:1 disable, 31,1 enable */
    // asm volatile("msr pmcntenset_el0, %0" : : "r" (ARMV8_PMCNTENSET_EL0_ENABLE));
    uint32_t r = 0;
    asm volatile("mrs %0, pmcntenset_el0" : "=r" (r));
    asm volatile("msr pmcntenset_el0, %0" : : "r" (r|1));
    asm volatile("msr pmcntenset_el0, %0" : : "r" (r|2));

    uint32_t r1 = 0;

    for (int i = 0; i < 100; i++) {
        printf_("This is a random loop\n");
    }
    asm volatile("mrs %0, pmevcntr0_el0" : "=r" (r));
    printf_("This is the current event counter 0: %d\n", r);

    asm volatile("mrs %0, pmevcntr1_el0" : "=r" (r1));
    printf_("This is the current event counter 1: %d\n", r1);

    asm volatile("mrs %0, pmccntr_el0" : "=r" (r));
    printf_("This is the current cycle counter: %d\n", r);

}

void init () {
    init_serial();

    // Attempt to read from the event status register
    enable_cycle_counter_el0();
    // Attempt to read any register on PMU
    uint32_t r;
    asm volatile("mrs %0, pmevcntr0_el0" : "=r" (r));

    asm volatile("mrs %0, pmccntr_el0" : "=r" (r));
    printf_("This is the current cycle counter: %d\n", r);

    init_pmu_event();
}

void notified(sel4cp_channel ch) {
    serial_server_printf("Entering notified in our profiler\n");
    if (ch == 1) {
        printf_("profiler recieved interrupt\n");
        // This is the irq case
        // sel4cp_irq_ack(ch);

    }
}