#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "snapshot.h"
#include "timer.h"
#include "profiler_config.h"
#include "client.h"
#include <sddf/network/shared_ringbuffer.h>
#include <sddf/util/printf.h>

uintptr_t uart_base;

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_free;
uintptr_t profiler_mem;

uintptr_t log_buffer;

ring_handle_t profiler_ring;

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
    seL4_ARM_PMUControl_CounterControl(PMU_CONTROL_CAP, 0);
}

/* Resume the PMU */
void resume_pmu() {
    seL4_ARM_PMUControl_CounterControl(PMU_CONTROL_CAP, 1);
}

// Configure event counter 0
void configure_cnt0(uint32_t event, uint32_t val) {
    seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 0, 0xffffffff - val, event);
}

// Configure event counter 1
void configure_cnt1(uint32_t event, uint32_t val) {
    seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 1, 0xffffffff - val, event);
}

// Configure event counter 2
void configure_cnt2(uint32_t event, uint32_t val) {
    seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 2, 0xffffffff - val, event);
}

// Configure event counter 3
void configure_cnt3(uint32_t event, uint32_t val) {
    seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 3, 0xffffffff - val, event);
}

// Configure event counter 4
void configure_cnt4(uint32_t event, uint32_t val) {
    seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 4, 0xffffffff - val, event);
}

// Configure event counter 5
void configure_cnt5(uint32_t event, uint32_t val) {
    seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 5, 0xffffffff - val, event);
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
        seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 6, init_cnt, 0);
        pmu_registers[CYCLE_CTR].overflowed = 0;
    }

}

/* Configure cycle counter*/
void configure_clkcnt(uint64_t val, bool sampling) {
    // Update the struct
    pmu_registers[CYCLE_CTR].count = val;
    pmu_registers[CYCLE_CTR].sampling = sampling;

    uint64_t init_cnt = 0xffffffffffffffff - pmu_registers[CYCLE_CTR].count;
    seL4_ARM_PMUControl_WriteEventCounter(PMU_CONTROL_CAP, 6, init_cnt, 0);
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

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_sample(microkit_id id, uint32_t time, uint64_t pc, uint64_t nr, uint32_t irqFlag, uint64_t *cc, uint64_t period) {

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
    temp_sample->cpu = 0;
    temp_sample->period = period;
    temp_sample->nr = nr;
    for (int i = 0; i < SEL4_PROF_MAX_CALL_DEPTH; i++) {
        temp_sample->ips[i] = 0;
        temp_sample->ips[i] = cc[i];
    }

    ret = enqueue_used(&profiler_ring, buffer);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts("Failed to dequeue from profiler used ring\n");
        return;
    }

    // Check if the buffers are full (for testing dumping when we have 10 buffers)
    // Notify the client that we need to dump. If we are dumping, do not
    // restart the PMU until we have managed to purge all buffers over the network.
    if (ring_empty(profiler_ring.free_ring)) {
        reset_pmu();
        halt_pmu();
        microkit_notify(CLIENT_PROFILER_CH);
    } else {
        reset_pmu();
        resume_pmu();
    }
}

/* Dump the values of the cycle counter and event counters 0 to 5*/
// void print_pmu_debug() {
//     uint64_t clock = 0;
//     uint32_t c1 = 0;
//     uint32_t c2 = 0;
//     uint32_t c3 = 0;
//     uint32_t c4 = 0;
//     uint32_t c5 = 0;
//     uint32_t c6 = 0;

//     ISB;
//     MRS(PMU_CYCLE_CTR, clock);
//     ISB;
//     MRS(PMU_EVENT_CTR0, c1);
//     ISB;
//     MRS(PMU_EVENT_CTR1, c2);
//     ISB;
//     MRS(PMU_EVENT_CTR2, c3);
//     ISB;
//     MRS(PMU_EVENT_CTR3, c4);
//     ISB;
//     MRS(PMU_EVENT_CTR4, c5);
//     ISB;
//     MRS(PMU_EVENT_CTR5, c6);

//     microkit_dbg_puts("This is the current cycle counter: ");
//     puthex64(clock);
//     microkit_dbg_puts("\nThis is the current event counter 0: ");
//     puthex64(c1);
//     microkit_dbg_puts("\nThis is the current event counter 1: ");
//     puthex64(c2);
//     microkit_dbg_puts("\nThis is the current event counter 2: ");
//     puthex64(c3);
//     microkit_dbg_puts("\nThis is the current event counter 3: ");
//     puthex64(c4);
//     microkit_dbg_puts("\nThis is the current event counter 4: ");
//     puthex64(c5);
//     microkit_dbg_puts("\nThis is the current event counter 5: ");
//     puthex64(c6);
//     microkit_dbg_puts("\n");
// }

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

    // Init the record buffers
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 512);

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        buff_desc_t buffer;
        buffer.phys_or_offset = profiler_mem + (i * sizeof(prof_sample_t));
        buffer.len = sizeof(prof_sample_t);
        int ret = enqueue_free(&profiler_ring, buffer);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to populate buffers for the perf record ring buffer\n");
            break;
        }
    }

    #ifdef CONFIG_PROFILER_ENABLE
    int res_buf = seL4_BenchmarkSetLogBuffer(log_buffer);
    if (res_buf) {
        microkit_dbg_puts("Could not set log buffer");
        puthex64(res_buf);
    }
    #endif

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
    pmu_sample_t *profLogs = (pmu_sample_t *) log_buffer;
    pmu_sample_t profLog = profLogs[0];

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

seL4_MessageInfo_t protected(microkit_channel ch, microkit_msginfo msginfo) {
    /* This is how the profiler main thread sends control commands */
    switch(microkit_msginfo_get_label(msginfo)) {
        case PROFILER_START:
            profiler_state = PROF_START;
            sddf_dprintf("Starting PMU\n");
            resume_pmu();
            break;
        case PROFILER_STOP:
            profiler_state = PROF_HALT;
            sddf_dprintf("Stopping PMU\n");
            halt_pmu();
            /* Purge buffers to main first */
            microkit_notify(CLIENT_PROFILER_CH);
            break;
        case PROFILER_RESTART:
            /* Only restart PMU if we haven't halted */
            if (profiler_state == PROF_START) {
                reset_pmu();
                resume_pmu();
            }
            break;
        default:
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts(": Invalid ppcall to profiler thread!\n");
            break;
    }
    return microkit_msginfo_new(0,0);
}


void notified(microkit_channel ch) {
    if (ch == 21) {
        // Get the interrupt flag from the PMU
        // @kwinter: need to make sure that this actually works. We are masking the PMU interrupt register in the kernel now.
        seL4_ARM_PMUControl_InterruptValue_t ret = seL4_ARM_PMUControl_InterruptValue(PMU_CONTROL_CAP);

        handle_irq(ret.interrupt_val);

        microkit_irq_ack(ch);
    }
}
