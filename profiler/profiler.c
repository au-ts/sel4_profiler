#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <sel4/profiler_types.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "profiler_config.h"
#include "client.h"
#include "config.h"
#include <sddf/util/printf.h>
#include <sddf/network/queue.h>

uintptr_t uart_base;

net_queue_handle_t profiler_queue;
net_queue_t *profiler_free;
net_queue_t *profiler_active;
uintptr_t profiler_data_region;
size_t profiler_queue_capacity = 0;
uintptr_t log_buffer;

__attribute__((__section__(".profiler_config"))) profiler_config_t config;

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
    MSR(PMU_EVENT_TYP5, event);}

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
        sddf_dprintf("Invalid register index\n");
        return;
    }
    pmu_registers[cntr].event = event;
    pmu_registers[cntr].count = val;
    pmu_registers[cntr].sampling = sampling;

    // Now write to register
    pmu_registers[cntr].config_ctr(event, val);
}

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_sample(microkit_child id, uint32_t time, uint64_t pc, uint64_t nr, uint32_t irqFlag, uint64_t *cc, uint64_t period) {

    net_buff_desc_t buffer;
    int ret = net_dequeue_free(&profiler_queue, &buffer);
    if (ret != 0) {
        sddf_dprintf(microkit_name);
        sddf_dprintf("Failed to dequeue from profiler free ring\n");
        return;
    }

    prof_sample_t *temp_sample = (prof_sample_t *) buffer.io_or_offset;

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

    ret = net_enqueue_active(&profiler_queue, buffer);

    if (ret != 0) {
        sddf_dprintf(microkit_name);
        sddf_dprintf("Failed to dequeue from profiler used ring\n");
        return;
    }

    // Check if the buffers are full (for testing dumping when we have 10 buffers)
    // Notify the client that we need to dump. If we are dumping, do not
    // restart the PMU until we have managed to purge all buffers over the network.
    if (net_queue_empty_free(&profiler_queue)) {
        reset_pmu();
        halt_pmu();
        microkit_notify(config.ch);
    } else {
        reset_pmu();
        resume_pmu();
    }
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
    sddf_dprintf("Profiler intialising...\n");

    // First, we must mark each of the TCB's that we are profiling with the
    // seL4_TCBFlag_profile flag.

    for (int i = 0; i < config.num_cli; i++) {
        seL4_TCB_SetFlags_t ret = seL4_TCB_SetFlags(BASE_TCB_CAP + i, 0, seL4_TCBFlag_profile);
        sddf_dprintf("This is the TCB flag for %d: %b\n", i, ret.flags);
    }

    // Ensure that the PMU is not running
    halt_pmu();

    // Init the record buffers
    net_queue_init(&profiler_queue, config.profiler_ring_free.vaddr, config.profiler_ring_used.vaddr, 512);

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        net_buff_desc_t buffer;
        buffer.io_or_offset = (uint64_t)config.profiler_mem.vaddr + (i * sizeof(prof_sample_t));
        buffer.len = sizeof(prof_sample_t);
        int ret = net_enqueue_free(&profiler_queue, buffer);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to populate buffers for the perf record ring buffer\n");
            break;
        }
    }

    #ifdef CONFIG_PROFILER_ENABLE
    int res_buf = seL4_BenchmarkSetLogBuffer(log_buffer);
    if (res_buf) {
        sddf_dprintf("Could not set log buffer");
        puthex64(res_buf);
    }
    #endif

    init_pmu_regs();

    /* INITIALISE WHAT COUNTERS WE WANT TO TRACK IN HERE */
    configure_clkcnt(CYCLE_COUNTER_PERIOD, 1);
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
            microkit_dbg_puts("Starting PMU\n");
            resume_pmu();
            break;
        case PROFILER_STOP:
            profiler_state = PROF_HALT;
            sddf_dprintf("Stopping PMU\n");
            halt_pmu();
            /* Purge buffers to main first */
            microkit_notify(config.ch);
            break;
        case PROFILER_RESTART:
            /* Only restart PMU if we haven't halted */
            if (profiler_state == PROF_START) {
                reset_pmu();
                resume_pmu();
            }
            break;
        default:
            sddf_dprintf(microkit_name);
            sddf_dprintf(": Invalid ppcall to profiler thread!\n");
            break;
    }
    return microkit_msginfo_new(0,0);
}


void notified(microkit_channel ch) {
    // sddf_dprintf("Got a notification\n");
    if (ch == 21) {
        // Get the interrupt flag from the PMU
        uint32_t irqFlag = 0;
        MRS(PMOVSCLR_EL0, irqFlag);

        // handle_irq(irqFlag);
        reset_pmu(irqFlag);
        resume_pmu();

        microkit_irq_ack(ch);
    }
}

seL4_Bool fault(microkit_child child, microkit_msginfo msginfo, microkit_msginfo *reply_msginfo) {
    sddf_dprintf("Received a fault!\n");
    size_t label = microkit_msginfo_get_label(msginfo);

    if (label == seL4_Fault_PMUEvent) {
        sddf_dprintf("AND ITS A PMU FAULT!!!\n");
        // Need to figure out how to get the whole 64 bit PC??
        uint64_t pc = microkit_mr_get(0);
        uint64_t fp = microkit_mr_get(1);
        sddf_dprintf("this was the PC: %p and this was the child id: %p and this was the fp: %p\n", pc, child, fp);
    }

    *reply_msginfo = microkit_msginfo_new(0 ,0);
    return seL4_True;

}