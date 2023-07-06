#include <stdbool.h>
#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "printf.h"
#include "snapshot.h"



uintptr_t uart_base;

// Global snapshot array, this needs to be a ring buffer in teh future

pmu_snapshot_t snapshot_arr[1000];
int snapshot_arr_top;

static void
enable_cycle_counter()
{
	uint64_t init_cnt = 0;
    uint64_t val;


	asm volatile("mrs %0, pmcr_el0" : "=r" (val));

	val |= (BIT(0) | BIT(2));

    asm volatile("isb; msr pmcr_el0, %0" : : "r" (val));

    asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));
}

static void
init_pmu_event() {
    printf_("Initialising a PMU event\n");
    uint32_t evtCount = 0x03;
    uint32_t evtCount2 = 0x04;
    /* Setup PMU counter to record specific event */
    /* evtCount is the event id */
    asm volatile("msr pmevcntr0_el0, %0" : : "r" (0xfffffff0));
    uint32_t temp;
    asm volatile("mrs %0, pmevcntr0_el0" : "=r" (temp));
    printf_("This is the current event counter 0 before: %d\n", temp);
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

    uint32_t r1 = 0;;

    asm volatile("mrs %0, pmevcntr0_el0" : "=r" (r));
    printf_("This is the current event counter 0: %d\n", r);

    asm volatile("mrs %0, pmevcntr1_el0" : "=r" (r1));
    printf_("This is the current event counter 1: %d\n", r1);

    asm volatile("mrs %0, pmccntr_el0" : "=r" (r));
    printf_("This is the current cycle counter: %d\n", r);

}

void print_snapshot() {
    uint64_t rr = 0;
    uint32_t r = 0;

    asm volatile("isb; mrs %0, pmccntr_el0" : "=r" (rr));
    printf_("This is the current cycle counter: %lu\n", rr);

    asm volatile("isb; mrs %0, pmevcntr0_el0" : "=r" (r));
    printf_("This is the current event counter 0: %d\n", r);
    asm volatile("isb; mrs %0, pmevcntr1_el0" : "=r" (r));
    printf_("This is the current event counter 1: %d\n", r);
    asm volatile("isb; mrs %0, pmevcntr2_el0" : "=r" (r));
    printf_("This is the current event counter 2: %d\n", r);
    asm volatile("isb; mrs %0, pmevcntr3_el0" : "=r" (r));
    printf_("This is the current event counter 3: %d\n", r);
    asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (r));
    printf_("This is the current event counter 4: %d\n", r);
}

/* Reset the cycle counter to the sampling period. This needs to be changed
to allow sampling on other event counters. */
void reset_cnt() {
    uint64_t init_cnt = 0xffffffffffffffff - SAMPLING_PERIOD;
    asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));
}

static inline int armv8pmu_has_overflowed(uint32_t pmovsr)
{
	return pmovsr & ARMV8_OVSR_MASK;
}

static inline uint32_t armv8pmu_getreset_flags(void)
{
	uint32_t value;

	/* Read */
	asm volatile("mrs %0, pmovsclr_el0" : "=r" (value));

	/* Write to clear flags */
	value &= ARMV8_OVSR_MASK;
	asm volatile("msr pmovsclr_el0, %0" :: "r" (value));

	return value;
}

/* Halt the PMU */
void halt_cnt() {
    asm volatile("msr pmcntenset_el0, %0" :: "r"(0 << 31));
}

/* Resume the PMU */
void resume_cnt() {
    asm volatile("msr pmcntenset_el0, %0" :: "r" BIT(31));
}

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_snapshot() {
    if (snapshot_arr_top < 100) {
        pmu_snapshot_t new_entry;
        asm volatile("isb; mrs %0, pmccntr_el0" : "=r" (new_entry.clock));
        asm volatile("isb; mrs %0, pmevcntr0_el0" : "=r" (new_entry.cnt1));
        asm volatile("isb; mrs %0, pmevcntr1_el0" : "=r" (new_entry.cnt2));
        asm volatile("isb; mrs %0, pmevcntr2_el0" : "=r" (new_entry.cnt3));
        asm volatile("isb; mrs %0, pmevcntr3_el0" : "=r" (new_entry.cnt4));
        asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (new_entry.cnt5));
        snapshot_arr[snapshot_arr_top] = new_entry;
        snapshot_arr_top += 1;
    } else if (snapshot_arr_top == 100) {
        pmu_snapshot_t temp = snapshot_arr[55];
        printf_("This is the current cycle counter: %lu\n", temp.clock);

        printf_("This is the current event counter 0: %d\n", temp.cnt1);
        printf_("This is the current event counter 1: %d\n", temp.cnt2);
        printf_("This is the current event counter 2: %d\n", temp.cnt3);
        printf_("This is the current event counter 3: %d\n", temp.cnt4);
        printf_("This is the current event counter 4: %d\n", temp.cnt5);

        // Stall here. REMOVE
        while(1) {
            
        }

        snapshot_arr_top += 1;
    }
}

seL4_MessageInfo_t
protected(sel4cp_channel ch, sel4cp_msginfo msginfo)
{
    // This is very core platform specific for now. We may need to change this in the future

    // For now we should only be recieving on channel 5
    if (ch == 5) {
        // For now, we should have only 1 message 
        uint32_t config = sel4cp_mr_get(0);

        switch (config)
        {
        case PROFILER_START:
            // Start PMU
            printf_("Starting PMU\n");
            resume_cnt();
            break;
        case PROFILER_STOP:
            printf_("Stopping the PMU\n");
            halt_cnt();
            // Also want to flush out anything that may be left in the ring buffer
            break;
        default:
            break;
        }
    }
}

void init () {
    snapshot_arr_top = 0;
    
    init_serial();

    enable_cycle_counter();

    // Notifying our dummy program to start running. This is just an empty infinite loop.
    sel4cp_notify(5);
}

void notified(sel4cp_channel ch) {
    // We should only enter the notified function for our IRQ
    if (ch == 0) {
        // Halt the PMU
        halt_cnt();

        // IN HERE WE NEED A WAY TO GET THE INSTRUCTION POINTER OF WHERE WE WERE 
        // EXECUTING BEFORE THE OVERFLOW INTERRUPT OCCURED. THIS WILL MOST LIKELY
        // NEED CORE PLATFORM/KERNEL CHANGES.

        // Print over serial for now
        print_snapshot();
        // add_snapshot();

        // Get the reset flags
        uint32_t pmovsr = armv8pmu_getreset_flags();

        // Check if an overflow has occured

        if(armv8pmu_has_overflowed(pmovsr)) {
            printf_("PMU has overflowed\n");
        } else {
            printf_("PMU hasn't overflowed\n");
        }
        // Mask the interuppt flag
        asm volatile("msr PMOVSCLR_EL0, %0" : : "r" BIT(31));
        
        // Reset our cycle counter
        reset_cnt();
        // Resume cycle counter
        resume_cnt();
        // Ack the irq
        sel4cp_irq_ack(ch);
    }

}