#include <microkit.h>
#include <sel4/sel4.h>

#include <stdbool.h>
#include <stdint.h>

#include <sddf/util/string.h>
#include <sddf/util/printf.h>
#include <sddf/network/queue.h>
#include <sddf/timer/client.h>
#include <sddf/timer/config.h>

#include <vspace.h>
#include "profiler.h"
#include "client.h"
#include "config.h"

#define CYCLE_COUNTER_PERIOD 1.3e9

// The user provides the following mapping regions.
// The small mapping region must be of page_size 0x1000
// THe large mapping region must be of page_size 0x200000
uintptr_t small_mapping_mr;
uintptr_t large_mapping_mr;

net_queue_handle_t profiler_queue;
net_queue_t *profiler_free;
net_queue_t *profiler_active;
uintptr_t profiler_data_region;
size_t profiler_queue_capacity = 0;

__attribute__((__section__(".profiler_config"))) profiler_config_t config;
__attribute__((__section__(".timer_client_config"))) timer_client_config_t timer_config;

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
    uint32_t val = 0;
    /* Disable Performance Counter */
    MRS(PMCR_EL0, val);
    val &= ~PMCR_EN;
    MSR(PMCR_EL0, val);
}

/* Resume the PMU */
void resume_pmu() {
    uint64_t val;
    MRS(PMCR_EL0, val);
    val |= PMCR_EN;
    MSR(PMCR_EL0, val);
}

void enable_cycle_counter() {
    uint64_t val;
    MRS(PMCNTENSET_EL0, val);
    // Cycle counter control is at bit 31
    val |= BIT(31);
    MSR(PMCNTENSET_EL0, val);
}

void disable_cycle_counter() {
    uint64_t val;
    MRS(PMCNTENSET_EL0, val);
    // Cycle counter control is at bit 31
    val &= ~BIT(31);
    MSR(PMCNTENSET_EL0, val);
}

void enable_event_counter(uint32_t cnt) {
    if (cnt >= PMU_NUM_REGS) {
        sddf_dprintf("Tried to enable a non-existant counter! Counter: %d must be less than %d",
                        cnt, PMU_NUM_REGS);
    }
    uint64_t val;
    MRS(PMCNTENSET_EL0, val);
    val |= BIT(cnt);
    MSR(PMCNTENSET_EL0, val);
}

void disable_event_counter(uint32_t cnt) {
    if (cnt >= PMU_NUM_REGS) {
        sddf_dprintf("Tried to disable a non-existant counter! Counter: %d must be less than %d",
                        cnt, PMU_NUM_REGS);
    }
    uint64_t val;
    MRS(PMCNTENSET_EL0, val);
    val &= ~BIT(cnt);
    MSR(PMCNTENSET_EL0, val);
}

void write_counter_val(uint32_t cnt, uint32_t val) {
    MSR(PMSELR_EL0, cnt);
    MSR(PMXEVCNTR_EL0, val);
}

uint32_t read_counter_val(uint32_t cnt) {
    MSR(PMSELR_EL0, cnt);
    uint32_t val = 0;
    MRS(PMXEVCNTR_EL0, val);
}

void write_event_val(uint32_t cnt, uint32_t event) {
    MSR(PMSELR_EL0, cnt);
    MSR(PMXEVTYPER_EL0, cnt);
}

/* Configure cycle counter*/
void configure_clkcnt(uint64_t val, bool sampling) {
    sddf_dprintf("configuring the cycle counter\n");
    // Update the struct
    pmu_registers[CYCLE_CTR].count = val;
    pmu_registers[CYCLE_CTR].sampling = sampling;

    uint64_t init_cnt = UINT64_MAX - pmu_registers[CYCLE_CTR].count;
    MSR(PMU_CYCLE_CTR, init_cnt);
}

void reset_pmu(uint32_t irqFlag) {
    for (int i = 0; i < PMU_NUM_REGS; i++) {
        if ((irqFlag & (1 << i)) && pmu_registers[i].sampling) {
            write_counter_val(i, UINT32_MAX - pmu_registers[i].count);
            write_event_val(i, pmu_registers[i].event);
        }
        pmu_registers[i].overflowed = 0;
    }

    if (irqFlag & (1 << 31) && pmu_registers[CYCLE_CTR].sampling) {
        // Case for cycle counter!
        uint64_t cnt = UINT64_MAX - CYCLE_COUNTER_PERIOD;
        MSR(PMU_CYCLE_CTR, cnt);
    } else if (irqFlag & (1 << 31)) {
        pmu_registers[CYCLE_CTR].overflowed = 0;
    }
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
        sddf_dprintf("trying to dump queue!\n");
        microkit_notify(config.ch);
    }
}

int callstack_unwind(microkit_channel ch, uintptr_t fp_addr, int depth, uint64_t *callstack)
{
    int nr = 0;
    // Loop and read the start of the frame pointer, save the lr value and load the next fp
    for (int i = 0; i < depth; i++) {
        // The LR should be one word above the FP
        seL4_Word lr_addr = fp_addr + sizeof(seL4_Word);

        // We need to traverse the frame stack chain. We want to save the value of the LR in the frame
        // entry as part of our perf callchain, and then look at the next frame record.
        uint64_t lr = 0;
        uint64_t fp = 0;
        uint32_t err_lr  = libvspace_read_word(ch, lr_addr, &lr);
        uint32_t err_fp = libvspace_read_word(ch, fp_addr, &fp);
        if (err_lr == 0 && err_fp == 0) {
            // Set the fp value to the next frame entry
            fp_addr = fp;
            // If the fp is 0, then we have reached the end of the frame stack chain
            callstack[i] = lr;
            nr++;
            if (fp_addr == 0) {
                break;
            }
        } else {
            // If we are unable to read, then we have reached the end of our stack unwinding
            sddf_dprintf("Unable to read a word, we have reached the end of the call stack unwind\n");
            break;
        }
    }
    return nr;
}

void handle_event(microkit_channel child, uint32_t irqFlag, uint64_t pc, uint64_t fp, uint64_t time) {
    uint32_t period = 0;

    uint64_t callstack[PROF_MAX_CALL_DEPTH] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    int nr = callstack_unwind(child, fp, PROF_MAX_CALL_DEPTH, callstack);

    // Update structs to check what counters overflowed
    if (irqFlag & (pmu_registers[CYCLE_CTR].sampling << 31)) {
        period = pmu_registers[CYCLE_CTR].count;
        pmu_registers[CYCLE_CTR].overflowed = 1;
        add_sample(child, time, pc, nr, irqFlag, callstack, period);
    }

    for (int i = 0; i < PMU_NUM_REGS; i++) {
        if (irqFlag & (pmu_registers[i].sampling << i)) {
            period = pmu_registers[i].count;
            pmu_registers[i].overflowed = 1;
            add_sample(child, time, pc, nr, irqFlag, callstack, period);
        }
    }

    return;
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
                // reset_pmu();
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

void init () {
    sddf_dprintf("Profiler intialising...\n");

    // First, we must mark each of the TCB's that we are profiling with the
    // seL4_TCBFlag_profile flag.
    for (int i = 0; i < config.num_cli; i++) {
        seL4_TCB_SetFlags_t ret = seL4_TCB_SetFlags(BASE_TCB_CAP + i, 0, seL4_TCBFlag_profile);
        sddf_dprintf("This is the TCB flag for %d: %b\n", i, ret.flags);
    }

    // Setup the mapping regions for libvspace to use.
    libvspace_set_small_mapping_region(small_mapping_mr);
    libvspace_set_large_mapping_region(large_mapping_mr);

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

    /* INITIALISE WHAT COUNTERS WE WANT TO TRACK IN HERE */
    configure_clkcnt(CYCLE_COUNTER_PERIOD, true);
    // Make sure that the PMU is not running until we start
    halt_pmu();
    // Set the profiler state to init
    profiler_state = PROF_INIT;
}

void notified(microkit_channel ch) {
    sddf_dprintf("Got a notification\n");
    if (ch == 21) {
        // Get the interrupt flag from the PMU
        uint32_t irqFlag = 0;
        MRS(PMOVSCLR_EL0, irqFlag);
        // Write back irqFlag to clear interrupts
        MSR(PMOVSCLR_EL0, irqFlag);
        reset_pmu(irqFlag);
        resume_pmu();


        microkit_irq_ack(ch);
    }
}

seL4_Bool fault(microkit_child child, microkit_msginfo msginfo, microkit_msginfo *reply_msginfo) {
    sddf_dprintf("Received a fault!\n");
    size_t label = microkit_msginfo_get_label(msginfo);
    if (label == seL4_Fault_PMUEvent) {
        // Need to figure out how to get the whole 64 bit PC??
        uint64_t pc = microkit_mr_get(0);
        uint64_t fp = microkit_mr_get(1);
        // Get the interrupt flag from the PMU
        uint32_t irqFlag = 0;
        MRS(PMOVSCLR_EL0, irqFlag);
        // Write back irqFlag to clear interrupts
        MSR(PMOVSCLR_EL0, irqFlag);
        handle_event(child, irqFlag, pc, fp, sddf_timer_time_now(timer_config.driver_id));
        reset_pmu(irqFlag);
        resume_pmu();
    }

    // Acknowledge the interrupt!
    microkit_irq_ack(21);
    *reply_msginfo = microkit_msginfo_new(0 ,0);
    return true;
}