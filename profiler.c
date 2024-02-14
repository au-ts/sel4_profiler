#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "printf.h"
#include "snapshot.h"
#include "timer.h"
#include "profiler_config.h"
#include "client.h"

uintptr_t uart_base;
uintptr_t profiler_control;

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_free;
uintptr_t profiler_mem;

uintptr_t log_buffer;

ring_handle_t profiler_ring;

/* State of profiler */
int profiler_state;

#define MRS(reg, v)  asm volatile("mrs %x0," reg : "=r"(v))
#define MSR(reg, v)                                \
    do {                                           \
        uint64_t _v = v;                             \
        asm volatile("msr " reg ",%x0" :: "r" (_v));\
    }while(0)

/* Halt the PMU */
void halt_pmu() {
    uint32_t value = 0;
    uint32_t mask = 0;

    /* Disable Performance Counter */
    asm volatile("MRS %0, PMCR_EL0" : "=r" (value));
    mask = 0;
    mask |= (1 << 0); /* Enable */
    mask |= (1 << 1); /* Cycle counter reset */
    mask |= (1 << 2); /* Reset all counters */
    asm volatile("MSR PMCR_EL0, %0" : : "r" (value & ~mask));

    /* Disable cycle counter register */
    asm volatile("MRS %0, PMCNTENSET_EL0" : "=r" (value));
    mask = 0;
    mask |= (1 << 31);
    asm volatile("MSR PMCNTENSET_EL0, %0" : : "r" (value & ~mask));
}

/* Resume the PMU */
void resume_pmu() {
    uint64_t val;

	asm volatile("mrs %0, pmcr_el0" : "=r" (val));

    val |= BIT(0);

    asm volatile("isb; msr pmcr_el0, %0" : : "r" (val));

    asm volatile("MSR PMCNTENSET_EL0, %0" : : "r" (BIT(31)));
}

/* Reset the cycle counter to the sampling period. This needs to be changed
to allow sampling on other event counters. */
void reset_pmu(uint32_t interrupt_flags) {
    // Go through all of the interrupt flags, and reset the appropriate counters to
    // the appropriate values. 
    if (interrupt_flags & BIT(0)) {
        uint32_t val = 0;

        if (IRQ_COUNTER0 == 1) {
            val = 0xffffffff - COUNTER0_PERIOD;
        }

        asm volatile("msr pmevcntr0_el0, %0" : : "r" (val));
    } 

    if (interrupt_flags & (BIT(1))) {
        uint32_t val = 0;

        if (IRQ_COUNTER1 == 1) {
            val = 0xffffffff - COUNTER1_PERIOD;
        }
        asm volatile("msr pmevcntr1_el0, %0" : : "r" (val));
    } 
    if (interrupt_flags & (BIT(2))) {
        uint32_t val = 0;

        if (IRQ_COUNTER2 == 1) {
            val = 0xffffffff - COUNTER2_PERIOD;
        }
        asm volatile("msr pmevcntr2_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(3))) {
        uint32_t val = 0;

        if (IRQ_COUNTER3 == 1) {
            val = 0xffffffff - COUNTER3_PERIOD;
        }
        asm volatile("msr pmevcntr3_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(4))) {
        uint32_t val = 0;

        if (IRQ_COUNTER4 == 1) {
            val = 0xffffffff - COUNTER4_PERIOD;
        }
        asm volatile("msr pmevcntr4_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(5))) {
        uint32_t val = 0;

        if (IRQ_COUNTER5 == 1) {
            val = 0xffffffff - COUNTER5_PERIOD;
        }
        asm volatile("msr pmevcntr5_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(31))) {
        uint64_t init_cnt = 0;
        if (IRQ_CYCLE_COUNTER == 1) {
            init_cnt = 0xffffffffffffffff - CYCLE_COUNTER_PERIOD;
        }

        asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));
    }

}

/* Configure cycle counter*/
void configure_clkcnt() {
    uint64_t init_cnt = 0xffffffffffffffff - CYCLE_COUNTER_PERIOD;
    asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));
}

/* Configure event counter 0 */
void configure_cnt0(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper0_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr0_el0, %0" : : "r" (val));
}

/* Configure event counter 1 */
void configure_cnt1(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper1_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr1_el0, %0" : : "r" (val));
}

/* Configure event counter 2 */
void configure_cnt2(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper2_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr2_el0, %0" : : "r" (val));
}

/* Configure event counter 3 */
void configure_cnt3(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper3_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr3_el0, %0" : : "r" (val));
}

/* Configure event counter 4 */
void configure_cnt4(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper4_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr4_el0, %0" : : "r" (val));
}

/* Configure event counter 5 */
void configure_cnt5(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper5_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr5_el0, %0" : : "r" (val));
}

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_sample(microkit_id id, uint32_t time, uint64_t pc, uint64_t nr, uint32_t pmovsr, uint64_t *cc) {    
    // microkit_dbg_puts("adding sample\n");
    uintptr_t buffer = 0;
    unsigned int buffer_len = 0;
    void * cookie = 0;

    int ret = dequeue_free(&profiler_ring, &buffer, &buffer_len, &cookie);
    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts("Failed to dequeue from profiler free ring\n");
        return;
    }

    prof_sample_t *temp_sample = (prof_sample_t *) buffer;
    
    // Find which counter overflowed, and the corresponding period

    uint64_t period = 0;

    if (pmovsr & (IRQ_CYCLE_COUNTER << 31)) {
        period = CYCLE_COUNTER_PERIOD;
    } else if (pmovsr & (IRQ_COUNTER0 << 0)) {
        period = COUNTER0_PERIOD;
    } else if (pmovsr & (IRQ_COUNTER1 << 1)) {
        period = COUNTER1_PERIOD;
    } else if (pmovsr & (IRQ_COUNTER2 << 2)) {
        period = COUNTER2_PERIOD;
    } else if (pmovsr & (IRQ_COUNTER3 << 3)) {
        period = COUNTER3_PERIOD;
    } else if (pmovsr & (IRQ_COUNTER4 << 4)) {
        period = COUNTER4_PERIOD;
    } else if (pmovsr & (IRQ_COUNTER5 << 5)) {
        period = COUNTER5_PERIOD;
    }

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
    
    ret = enqueue_used(&profiler_ring, buffer, buffer_len, &cookie);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts("Failed to dequeue from profiler used ring\n");
        return;
    }

    // Check if the buffers are full (for testing dumping when we have 10 buffers)
    // Notify the client that we need to dump. If we are dumping, do not 
    // restart the PMU until we have managed to purge all buffers over the network.
    if (ring_empty(profiler_ring.free_ring)) {
        reset_pmu(pmovsr);
        halt_pmu();
        microkit_notify(CLIENT_CH);
    } else {
        reset_pmu(pmovsr);
        resume_pmu();
    }
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

    asm volatile("isb; mrs %0, pmccntr_el0" : "=r" (clock));
    asm volatile("isb; mrs %0, pmevcntr0_el0" : "=r" (c1));
    asm volatile("isb; mrs %0, pmevcntr1_el0" : "=r" (c2));
    asm volatile("isb; mrs %0, pmevcntr2_el0" : "=r" (c3));
    asm volatile("isb; mrs %0, pmevcntr3_el0" : "=r" (c4));
    asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (c5));
    asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (c6));

    microkit_dbg_puts("This is the current cycle counter: ");
    puthex64(clock);
    microkit_dbg_puts("\nThis is the current event counter 0: ");
    puthex64(c1);
    microkit_dbg_puts("This is the current event counter 1: ");
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

void init () {
    int *prof_cnt = (int *) profiler_control;

    *prof_cnt = 0;

    // Init the record buffers
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 0, 512, 512);
    
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        int ret = enqueue_free(&profiler_ring, profiler_mem + (i * sizeof(prof_sample_t)), sizeof(prof_sample_t), NULL);
        
        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to populate buffers for the perf record ring buffer\n");
            break;
        }
    }

    #ifdef CONFIG_PROFILER_ENABLE

    int res_buf = seL4_BenchmarkSetLogBuffer(log_buffer);

    if (res_buf) {
        print("Could not set log buffer");
        puthex64(res_buf);
    } else {
        print("We set the log buffer\n");
    }
    #endif

    /* INITIALISE WHAT COUNTERS WE WANT TO TRACK IN HERE*/
    /* HERE USERS CAN ADD IN CONFIGURATION OPTIONS FOR THE PROFILER BY SETTING
    CALLING THE CONFIGURE F For example:

    configure_cnt0(L1I_CACHE_REFILL, 0xfffffff); 
    */
    // configure_cnt2(L1I_CACHE_REFILL, 0xffffff00); 
    // configure_cnt1(L1D_CACHE_REFILL, 0xfffffff0); 

    // Make sure that the PMU is not running until we start
    halt_pmu();

    // Set the profiler state to init
    profiler_state = PROF_INIT;
}

void handle_irq(uint32_t irqFlag) {
    pmu_sample_t *profLogs = (pmu_sample_t *) log_buffer;
    pmu_sample_t profLog = profLogs[0];

    if (profLog.valid == 1) {
        if (irqFlag & (IRQ_CYCLE_COUNTER << 31) ||
            irqFlag & (IRQ_COUNTER0 << 0) ||
            irqFlag & (IRQ_COUNTER1 << 1) ||
            irqFlag & (IRQ_COUNTER2 << 2) ||
            irqFlag & (IRQ_COUNTER3 << 3) ||
            irqFlag & (IRQ_COUNTER4 << 4) ||
            irqFlag & (IRQ_COUNTER5 << 5)) {
            add_sample(profLog.pid, profLog.time, profLog.ip, profLog.nr, irqFlag, profLog.ips);
        }
    } else {
        // Not a valid sample. Restart PMU.
        reset_pmu(irqFlag);
        resume_pmu();
    }
}

void notified(microkit_channel ch) {
    if (ch == 10) {
        microkit_dbg_puts("Starting PMU\n");
        // Set the profiler state to start
        profiler_state = PROF_START;
        configure_clkcnt();
        resume_pmu();
    } else if (ch == 20) {
        microkit_dbg_puts("Halting PMU\n");
        // Set the profiler state to halt
        profiler_state = PROF_HALT;
        halt_pmu();
        // Purge any buffers that may be leftover
        microkit_notify(CLIENT_CH);
    } else if (ch == 30) {
        // Only resume if profiler state is in 'START' state
        if (profiler_state == PROF_START) {
            // microkit_dbg_puts("Resuming PMU\n");
            resume_pmu();
        }
    } else if (ch == 21) {
        // Get the interrupt flag from the PMU
        // microkit_dbg_puts("Recv pmu irq\n");
        uint32_t pmovsr = 0;
        MRS("PMOVSCLR_EL0", pmovsr);

        handle_irq(pmovsr);

        // Clear the interrupt flag 
        uint32_t val = BIT(31);
        MSR("PMOVSCLR_EL0", val);

        microkit_irq_ack(ch);
    }
}
