#include <stdbool.h>
#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "printf.h"


uintptr_t uart_base;

static void
enable_cycle_counter_el0()
{
	uint64_t val;
	// /* Disable cycle counter overflow interrupt */
	// asm volatile("msr pmintenclr_el1, %0" : : "r" ((uint64_t)(1 << 31)));
	// /* Enable cycle counter */
	asm volatile("msr pmcntenset_el0, %0" :: "r" BIT(31));
    /* Enable user-mode access to cycle counters. */
	// asm volatile("msr pmuserenr_el0, %0" : : "r"(BIT(0) | BIT(2)));
    serial_server_printf("Finished enabling the clock counter\n");
	/* Clear cycle counter and start */
	asm volatile("mrs %0, pmcr_el0" : "=r" (val));
	val |= (BIT(0) | BIT(2));
    serial_server_printf("Cleared the counter, and starting it.\n");
	asm volatile("isb" : :);
    asm volatile("msr pmcr_el0, %0" : : "r" (val));
	val = BIT(27);
	asm volatile("msr pmccfiltr_el0, %0" : : "r" (val));
    uint32_t r;
    asm volatile("mrs %0, pmccntr_el0" : "=r" (r));
    printf_("This is the current cycle counter: %d\n", r);
}

void init () {
    init_serial();
    serial_server_printf("Initing the serial server connection\n");

    serial_server_printf("We are initing the profiler\n");


    // Attempt to read from the event status register
    enable_cycle_counter_el0();
    // Attempt to read any register on PMU
    uint32_t r;
    asm volatile("mrs %0, pmevcntr0_el0" : "=r" (r));

    serial_server_printf("Enabled the cycle counter\n");
    asm volatile("mrs %0, pmccntr_el0" : "=r" (r));
    printf_("This is the current cycle counter: %d\n", r);
}

void notified(sel4cp_channel ch) {
    serial_server_printf("Entering notified in our profiler\n");
}