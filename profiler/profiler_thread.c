#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "snapshot.h"
#include "timer.h"
#include "profiler_config.h"

#define ADD_SAMPLE_CH 1

uintptr_t uart_base;
// Array of pmu registers available
pmu_reg_t pmu_registers[PMU_NUM_REGS];

/* State of profiler */
int profiler_state;

#define ISB asm volatile("isb")
#define MRS(reg, v)  asm volatile("mrs %x0," reg : "=r"(v))
#define MSR(reg, v)                                \
    do {                                           \
        uint64_t _v = v;                             \
        asm volatile("msr " reg ",%x0" ::  "r" (_v));\
    }while(0)

/* Halt the PMU */
void halt_pmu() {
    uint32_t value = 0;
    uint32_t mask = 0;

    /* Disable Performance Counter */
    MRS(PMCR_EL0, value);
    mask = 0;
    mask |= (1 << 0); /* Enable */
    mask |= (1 << 1); /* Cycle counter reset */
    mask |= (1 << 2); /* Reset all counters */
    MSR(PMCR_EL0, (value & ~mask));

    /* Disable cycle counter register */
    MRS(PMCNTENSET_EL0, value);
    mask = 0;
    mask |= (1 << 31);
    MSR(PMCNTENSET_EL0, (value & ~mask));
}

/* Resume the PMU */
void resume_pmu() {
    uint64_t val;

    MRS(PMCR_EL0, val);

    val |= BIT(0);

    ISB;
    MSR(PMCR_EL0, val);

    MSR(PMCNTENSET_EL0, (BIT(31)));
}

// Configure event counter 0
void configure_cnt0(uint32_t event, uint32_t val) {
    ISB;
    MSR(PMU_EVENT_CTR0, 0xffffffff - val);
    MSR(PMU_EVENT_TYP0, event);
}

// Configure event counter 1
void configure_cnt1(uint32_t event, uint32_t val) {
    ISB;
    MSR(PMU_EVENT_CTR1, 0xffffffff - val);
    MSR(PMU_EVENT_TYP1, event);
}

// Configure event counter 2
void configure_cnt2(uint32_t event, uint32_t val) {
    ISB;
    MSR(PMU_EVENT_CTR2, 0xffffffff - val);
    MSR(PMU_EVENT_TYP2, event);
}

// Configure event counter 3
void configure_cnt3(uint32_t event, uint32_t val) {
    ISB;
    MSR(PMU_EVENT_CTR3, 0xffffffff - val);
    MSR(PMU_EVENT_TYP3, event);
}

// Configure event counter 4
void configure_cnt4(uint32_t event, uint32_t val) {
    ISB;
    MSR(PMU_EVENT_CTR4, 0xffffffff - val);
    MSR(PMU_EVENT_TYP4, event);
}

// Configure event counter 5
void configure_cnt5(uint32_t event, uint32_t val) {
    ISB;
    MSR(PMU_EVENT_CTR5, 0xffffffff - val);
    MSR(PMU_EVENT_TYP5, event);
}

void reset_pmu() {
    // Loop through the pmu registers, if the overflown flag has been set,
    // and we are sampling on this register, reset to max value - count.
    // Otherwise, reset to 0.
    for (int i = 0; i < PMU_NUM_REGS - 1; i++) {
        if (pmu_registers[i].overflowed == 1 && pmu_registers[i].sampling == 1) {
            pmu_registers[i].config_ctr(pmu_registers[i].event, pmu_registers[i].count);
            pmu_registers[i].overflowed = 0;
        } else if (pmu_registers[i].overflowed == 1) {
            pmu_registers[i].config_ctr(pmu_registers[i].event, 0xffffffff);
            pmu_registers[i].overflowed = 0;
        }
    }

    // Handle the cycle counter.
    if (pmu_registers[CYCLE_CTR].overflowed == 1) {
        uint64_t init_cnt = 0;
        if (pmu_registers[CYCLE_CTR].sampling == 1) {
            init_cnt = 0xffffffffffffffff - CYCLE_COUNTER_PERIOD;
        }
        MSR(PMU_CYCLE_CTR, init_cnt);
        pmu_registers[CYCLE_CTR].overflowed = 0;
    }

}

/* Configure cycle counter*/
void configure_clkcnt(uint64_t val, bool sampling) {
    // Update the struct
    pmu_registers[CYCLE_CTR].count = val;
    pmu_registers[CYCLE_CTR].sampling = sampling;

    uint64_t init_cnt = 0xffffffffffffffff - pmu_registers[CYCLE_CTR].count;
    MSR(PMU_CYCLE_CTR, init_cnt);
}

void configure_eventcnt(int cntr, uint32_t event, uint64_t val, bool sampling) {
    // First update the struct
    if (cntr < 0 || cntr >= PMU_NUM_REGS) {
        microkit_dbg_puts("Invalid register index\n");
        return;
    }
    pmu_registers[cntr].event = event;
    pmu_registers[cntr].count = val;
    pmu_registers[cntr].sampling = sampling;

    // Now write to register
    pmu_registers[cntr].config_ctr(event, val);
}


/* Dump the values of the cycle counter and event counters 0 to 5*/
void print_pmu_debug() {
    uint64_t clock = 0;
    uint32_t c1 = 0;
    uint32_t c2 = 0;
    uint32_t c3 = 0;
    uint32_t c4 = 0;
    uint32_t c5 = 0;
    uint32_t c6 = 0;

    ISB;
    MRS(PMU_CYCLE_CTR, clock);
    ISB;
    MRS(PMU_EVENT_CTR0, c1);
    ISB;
    MRS(PMU_EVENT_CTR1, c2);
    ISB;
    MRS(PMU_EVENT_CTR2, c3);
    ISB;
    MRS(PMU_EVENT_CTR3, c4);
    ISB;
    MRS(PMU_EVENT_CTR4, c5);
    ISB;
    MRS(PMU_EVENT_CTR5, c6);

    microkit_dbg_puts("This is the current cycle counter: ");
    puthex64(clock);
    microkit_dbg_puts("\nThis is the current event counter 0: ");
    puthex64(c1);
    microkit_dbg_puts("\nThis is the current event counter 1: ");
    puthex64(c2);
    microkit_dbg_puts("\nThis is the current event counter 2: ");
    puthex64(c3);
    microkit_dbg_puts("\nThis is the current event counter 3: ");
    puthex64(c4);
    microkit_dbg_puts("\nThis is the current event counter 4: ");
    puthex64(c5);
    microkit_dbg_puts("\nThis is the current event counter 5: ");
    puthex64(c6);
    microkit_dbg_puts("\n");
}

void init_pmu_regs() {
    /* Initialise the register array */
    pmu_registers[0].config_ctr = configure_cnt0;
    pmu_registers[0].count = 0;
    pmu_registers[0].event = 0;
    pmu_registers[0].sampling = 0;
    pmu_registers[0].overflowed = 0;
    pmu_registers[1].config_ctr = configure_cnt1;
    pmu_registers[1].count = 0;
    pmu_registers[1].event = 0;
    pmu_registers[1].sampling = 0;
    pmu_registers[1].overflowed = 0;
    pmu_registers[2].config_ctr = configure_cnt2;
    pmu_registers[2].count = 0;
    pmu_registers[2].event = 0;
    pmu_registers[2].sampling = 0;
    pmu_registers[2].overflowed = 0;
    pmu_registers[3].config_ctr = configure_cnt3;
    pmu_registers[3].count = 0;
    pmu_registers[3].event = 0;
    pmu_registers[3].sampling = 0;
    pmu_registers[3].overflowed = 0;
    pmu_registers[4].config_ctr = configure_cnt4;
    pmu_registers[4].count = 0;
    pmu_registers[4].event = 0;
    pmu_registers[4].sampling = 0;
    pmu_registers[4].overflowed = 0;
    pmu_registers[5].config_ctr = configure_cnt5;
    pmu_registers[5].count = 0;
    pmu_registers[5].event = 0;
    pmu_registers[5].sampling = 0;
    pmu_registers[5].overflowed = 0;
    pmu_registers[CYCLE_CTR].count = 0;
    pmu_registers[CYCLE_CTR].event = 0;
    pmu_registers[CYCLE_CTR].sampling = 0;
    pmu_registers[CYCLE_CTR].overflowed = 0;
}

void init () {
    microkit_dbg_puts("Profiler intialising...\n");

    // Ensure that the PMU is not running
    halt_pmu();

    init_pmu_regs();

    /* INITIALISE WHAT COUNTERS WE WANT TO TRACK IN HERE */

    configure_clkcnt(CYCLE_COUNTER_PERIOD, 1);
    // configure_eventcnt(EVENT_CTR_0, L1D_CACHE, 16, 1);

    // Make sure that the PMU is not running until we start
    halt_pmu();

    // Set the profiler state to init
    profiler_state = PROF_INIT;
}

void handle_irq(uint32_t irqFlag) {
    // Update structs to check what counters overflowed
    if (irqFlag & (pmu_registers[CYCLE_CTR].sampling << 31)) {
        pmu_registers[CYCLE_CTR].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[0].sampling << 0)) {
        pmu_registers[0].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[1].sampling << 1)) {
        pmu_registers[1].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[2].sampling << 2)) {
        pmu_registers[2].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[3].sampling << 3)) {
        pmu_registers[3].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[4].sampling << 4)) {
        pmu_registers[4].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[5].sampling << 5)) {
        pmu_registers[5].overflowed = 1;
    }

    // Notify the main profiler thread that we have a sample for it to record
    microkit_notify(ADD_SAMPLE_CH);
}

void notified(microkit_channel ch) {
    if (ch == 10) {
        // Set the profiler state to start
        profiler_state = PROF_START;
        resume_pmu();
    } else if (ch == 20) {
        // Set the profiler state to halt
        profiler_state = PROF_HALT;
        halt_pmu();
    } else if (ch == 30) {
        // Only resume if profiler state is in 'START' state
        if (profiler_state == PROF_START) {
            resume_pmu();
        }
    } else if (ch == 21) {
        // Get the interrupt flag from the PMU
        uint32_t irqFlag = 0;
        MRS(PMOVSCLR_EL0, irqFlag);

        handle_irq(irqFlag);

        // Clear the interrupt flag
        uint32_t val = BIT(31);
        MSR(PMOVSCLR_EL0, val);

        microkit_irq_ack(ch);
    } else if (ch == ADD_SAMPLE_CH) {
        // We have been notified back by the main thread, restart profiling now
        reset_pmu();
        resume_pmu();
    }
}