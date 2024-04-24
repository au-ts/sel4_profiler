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
#include <sddf/network/shared_ringbuffer.h>
#include <sddf/util/printf.h>

#define MAIN_CH 1
#define NUM_BUFFERS 512
uintptr_t uart_base;
// Array of pmu registers available
pmu_reg_t pmu_registers[PMU_NUM_REGS];

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_free;
uintptr_t profiler_mem;
ring_handle_t profiler_ring;

uintptr_t log_buffer;

// This is set during init
int curr_cpu;

/* State of profiler */
int profiler_state;

#define ISB asm volatile("isb")
#define MRS(reg, v)  asm volatile("mrs %x0," reg : "=r"(v))
#define MSR(reg, v)                                \
    do {                                           \
        uint64_t _v = v;                             \
        asm volatile("msr " reg ",%x0" ::  "r" (_v));\
    }while(0)

static bool __str_match(const char *s0, const char *s1)
{
    while (*s0 != '\0' && *s1 != '\0' && *s0 == *s1) {
        s0++, s1++;
    }
    return *s0 == *s1;
}

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

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_sample(microkit_id id, uint32_t time, uint64_t pc, uint64_t nr, uint32_t irqFlag, uint64_t *cc, uint64_t period) {

    // sddf_dprintf("This is the size of the free ring: %d ---- This is the size of the used ring: %d\n", ring_size(profiler_ring.free_ring), ring_size(profiler_ring.used_ring));
    buff_desc_t buffer;
    int ret = dequeue_free(&profiler_ring, &buffer);
    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts("Failed to dequeue from profiler free ring\n");
        return;
    }

    prof_sample_t *temp_sample = (prof_sample_t *) buffer.phys_or_offset;

    // Find which counter overflowed, and the corresponding period

    temp_sample->ip = pc;
    temp_sample->pid = id;
    temp_sample->time = time;
    temp_sample->cpu = curr_cpu;
    temp_sample->period = period;
    temp_sample->nr = nr;
    for (int i = 0; i < SEL4_PROF_MAX_CALL_DEPTH; i++) {
        temp_sample->ips[i] = 0;
        temp_sample->ips[i] = cc[i];
    }

    ret = enqueue_used(&profiler_ring, buffer);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": Failed to dequeue from profiler used ring\n");
        return;
    }

    /*  Check if the buffers are full (for testing dumping when we have 10 buffers)
        Notify the client that we need to dump. If we are dumping, do not
        restart the PMU until we have managed to purge all buffers over the network. */
    if (ring_empty(profiler_ring.free_ring)) {
        reset_pmu();
        halt_pmu();
        microkit_notify(MAIN_CH);
    } else {
        reset_pmu();
        resume_pmu();
    }
}

void handle_irq(uint32_t irqFlag) {
    pmu_sample_t *profLogs = (pmu_sample_t *) log_buffer;
    pmu_sample_t profLog = profLogs[curr_cpu];

    uint64_t period = 0;

    // Update structs to check what counters overflowed
    if (irqFlag & (pmu_registers[CYCLE_CTR].sampling << 31)) {
        period = pmu_registers[CYCLE_CTR].count;
        pmu_registers[CYCLE_CTR].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[0].sampling << 0)) {
        period = pmu_registers[0].count;
        pmu_registers[0].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[1].sampling << 1)) {
        period = pmu_registers[1].count;
        pmu_registers[1].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[2].sampling << 2)) {
        period = pmu_registers[2].count;
        pmu_registers[2].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[3].sampling << 3)) {
        period = pmu_registers[3].count;
        pmu_registers[3].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[4].sampling << 4)) {
        period = pmu_registers[4].count;
        pmu_registers[4].overflowed = 1;
    }

    if (irqFlag & (pmu_registers[5].sampling << 5)) {
        period = pmu_registers[5].count;
        pmu_registers[5].overflowed = 1;
    }

    if (profLog.valid == 1) {
        if (irqFlag & (pmu_registers[CYCLE_CTR].sampling << 31) ||
            irqFlag & (pmu_registers[0].sampling << 0) ||
            irqFlag & (pmu_registers[1].sampling << 1) ||
            irqFlag & (pmu_registers[2].sampling << 2) ||
            irqFlag & (pmu_registers[3].sampling << 3) ||
            irqFlag & (pmu_registers[4].sampling << 4) ||
            irqFlag & (pmu_registers[5].sampling << 5)) {
            add_sample(profLog.pid, profLog.time, profLog.ip, profLog.nr, irqFlag, profLog.ips, period);
        }
    } else {
        // Not a valid sample. Restart PMU.
        reset_pmu(irqFlag);
        resume_pmu();
    }
}

void init () {
    microkit_dbg_puts("Profiler intialising...\n");

    // Ensure that the PMU is not running
    halt_pmu();

    init_pmu_regs();

    // Init the record buffers
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 512);

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        buff_desc_t buffer;
        buffer.phys_or_offset = profiler_mem + (i * sizeof(prof_sample_t));
        buffer.len = sizeof(prof_sample_t);
        int ret = enqueue_free(&profiler_ring, buffer);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts(": Failed to populate buffers for the perf record ring buffer\n");
            break;
        }
    }

    /* INITIALISE WHAT COUNTERS WE WANT TO TRACK IN HERE */

    configure_clkcnt(CYCLE_COUNTER_PERIOD, 1);
    // configure_eventcnt(EVENT_CTR_0, L1D_CACHE, 16, 1);

    // Make sure that the PMU is not running until we start
    halt_pmu();

    // Set the profiler state to init
    profiler_state = PROF_INIT;

    microkit_dbg_puts("this is the name of the profiler thread: ");
    microkit_dbg_puts(microkit_name);
    microkit_dbg_puts("\n");
    // TODO: Fix how we get CPU id
    if (__str_match(microkit_name, "profiler0")) {
        curr_cpu = 0;
    } else if (__str_match(microkit_name, "profiler1")) {
        curr_cpu = 1;
    } else {
        // Default to cpu 0
        curr_cpu = 0;
    }
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
    } else if (ch == MAIN_CH) {
        // We have been notified back by the main thread, restart profiling now
        reset_pmu();
        resume_pmu();
    }
}