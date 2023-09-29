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
#include "perf.h"
#include "timer.h"
#include "profiler_config.h"
#include "client.h"

uintptr_t uart_base;
uintptr_t profiler_control;

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_free;
uintptr_t profiler_mem;

ring_handle_t profiler_ring;

/* This global bit string allows us to keep track of what event counters
are being used. We use this bit string when enabling/disabling the PMU */
uint32_t active_counters = (BIT(31)); 

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_snapshot(sel4cp_id id,uint32_t time, uint64_t pc) {
    // This will be added to the raw data section
    pmu_snapshot_t new_entry;
    asm volatile("isb; mrs %0, pmccntr_el0" : "=r" (new_entry.clock));
    asm volatile("isb; mrs %0, pmevcntr0_el0" : "=r" (new_entry.cnt1));
    asm volatile("isb; mrs %0, pmevcntr1_el0" : "=r" (new_entry.cnt2));
    asm volatile("isb; mrs %0, pmevcntr2_el0" : "=r" (new_entry.cnt3));
    asm volatile("isb; mrs %0, pmevcntr3_el0" : "=r" (new_entry.cnt4));
    asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (new_entry.cnt5));
    asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (new_entry.cnt5));
    
    new_entry.pc = pc;
    
    uintptr_t buffer = 0;
    unsigned int buffer_len = 0;
    void * cookie = 0;

    int ret = dequeue_free(&profiler_ring, &buffer, &buffer_len, &cookie);
    if (ret != 0) {
        sel4cp_dbg_puts(sel4cp_name);
        sel4cp_dbg_puts("Failed to dequeue from profiler free ring\n");
        return;
    }
    perf_sample_t *temp_sample = (perf_sample_t *) buffer;

    temp_sample->ip = pc;
    temp_sample->pid = id;
    temp_sample->time = time;
    temp_sample->addr = 0;
    temp_sample->id = 0;
    temp_sample->stream_id = 0;
    temp_sample->cpu = 0;
    temp_sample->period = CYCLE_COUNTER_PERIOD;
    temp_sample->values = 0;

    // Need to sort this out
    temp_sample->data = &new_entry;

    ret = enqueue_used(&profiler_ring, buffer, buffer_len, &cookie);

    if (ret != 0) {
        sel4cp_dbg_puts(sel4cp_name);
        sel4cp_dbg_puts("Failed to dequeue from profiler used ring\n");
        return;
    }

    // Check if the buffers are full (for testing dumping when we have 10 buffers)
    // Notify the client that we need to dump
    if (ring_size(profiler_ring.used_ring) == 1) {
        sel4cp_notify(CLIENT_CH);
    }
}



/* Check if the PMU has overflowed. The following functions are currently 
only used for debugging purposes. */
static inline int pmu_has_overflowed(uint32_t pmovsr)
{
	return pmovsr & ARMV8_OVSR_MASK;
}

/* Get the reset flags after an interrupt has occured on the PMU */
static inline uint32_t pmu_getreset_flags(void)
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

    // Purge buffers 
    sel4cp_notify(CLIENT_CH);

}

/* Resume the PMU */
void resume_cnt() {
    uint64_t val;

	asm volatile("mrs %0, pmcr_el0" : "=r" (val));

    val |= BIT(0);

    asm volatile("isb; msr pmcr_el0, %0" : : "r" (val));

    asm volatile("MSR PMCNTENSET_EL0, %0" : : "r" (BIT(31)));

}

/* Reset the cycle counter to the sampling period. This needs to be changed
to allow sampling on other event counters. */
void reset_cnt(uint32_t interrupt_flags) {
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
void configure_clkcnt(uint64_t val) {
    uint64_t init_cnt = 0xffffffffffffffff - val;
    asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));
}


/* Configure event counter 0 */
void configure_cnt0(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper0_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr0_el0, %0" : : "r" (val));
    active_counters |= BIT(0);
}

/* Configure event counter 1 */
void configure_cnt1(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper1_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr1_el0, %0" : : "r" (val));
    active_counters |= BIT(1);
}

/* Configure event counter 2 */
void configure_cnt2(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper2_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr2_el0, %0" : : "r" (val));
    active_counters |= BIT(2);
}

/* Configure event counter 3 */
void configure_cnt3(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper3_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr3_el0, %0" : : "r" (val));
    active_counters |= BIT(3);
}

/* Configure event counter 4 */
void configure_cnt4(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper4_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr4_el0, %0" : : "r" (val));
    active_counters |= BIT(4);
}

/* Configure event counter 5 */
void configure_cnt5(uint32_t event, uint32_t val) {
    asm volatile("isb; msr pmevtyper5_el0, %0" : : "r" (event));
    asm volatile("msr pmevcntr5_el0, %0" : : "r" (val));
    active_counters |= BIT(5);
}

/* Initial user PMU configure interface */
void user_pmu_configure(pmu_config_args_t config_args) {
    uint32_t event = config_args.reg_event & ARMV8_PMEVTYPER_EVTCOUNT_MASK;
    // In each of these cases set event for counter, set value of counter.
    switch (config_args.reg_num)
    {
    case EVENT_CTR_0:
        configure_cnt0(event, config_args.reg_val);
        break;
    case EVENT_CTR_1:
        configure_cnt1(event, config_args.reg_val);
        break;
    case EVENT_CTR_2:
        configure_cnt2(event, config_args.reg_val);
        break;
    case EVENT_CTR_3:
        configure_cnt3(event, config_args.reg_val);
        break;
    case EVENT_CTR_4:
        configure_cnt4(event, config_args.reg_val);
        break;
    case EVENT_CTR_5:
        configure_cnt5(event, config_args.reg_val);
        break;
    default:
        break;
    }
}

/* PPC will arrive from application to configure the PMU */
seL4_MessageInfo_t
protected(sel4cp_channel ch, sel4cp_msginfo msginfo)
{
    // This is deprecated for now. Using a shared memory region between process and profiler to control/configure
    // For now we should only be recieving on channel 5
    if (ch == 5) {
        // For now, we should have only 1 message 
        uint32_t config = sel4cp_mr_get(0);

        switch (config)
        {
        case PROFILER_START:
            // Start PMU
            resume_cnt();
            break;
        case PROFILER_STOP:
            halt_cnt();
            // Also want to flush out anything that may be left in the ring buffer
            break;
        case PROFILER_CONFIGURE:
            {
            /* Example layout of what a call to profile configure should look like (Based on the perfmon2 spec):
                sel4cp_mr_set(0, PROFILER_CONFIGURE);
                sel4cp_mr_set(1, REG_NUM);
                sel4cp_mr_set(2, EVENT NUM);
                sel4cp_mr_set(3, FLAGS(empty for now));
                sel4cp_mr_set(4, VAL_UPPER);
                sel4cp_mr_set(5, VAL_LOWER);
            These message registers are then unpacted here and applied to the PMU state.     
            */

            pmu_config_args_t args;
            args.reg_num = sel4cp_mr_get(1);
            args.reg_event = sel4cp_mr_get(2);
            args.reg_flags = sel4cp_mr_get(3);
            uint32_t top = sel4cp_mr_get(4);
            uint32_t bottom = sel4cp_mr_get(5);
            args.reg_val = ((uint64_t) top << 32) | bottom;
            user_pmu_configure(args);
            break;
            }
        default:
            break;
        }
    }
    return sel4cp_msginfo_new(0, 0);
}

void init () {
    int *prof_cnt = (int *) profiler_control;

    *prof_cnt = 0;

    // Init the record buffers
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 0, 512, 512);
    
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        int ret = enqueue_free(&profiler_ring, profiler_mem + (i * sizeof(perf_sample_t)), sizeof(perf_sample_t), NULL);
        
        if (ret != 0) {
            sel4cp_dbg_puts(sel4cp_name);
            sel4cp_dbg_puts("Failed to populate buffers for the perf record ring buffer\n");
            break;
        }
    }


    /* INITIALISE WHAT COUNTERS WE WANT TO TRACK IN HERE*/
    /* HERE USERS CAN ADD IN CONFIGURATION OPTIONS FOR THE PROFILER BY SETTING
    CALLING THE CONFIGURE FUNCTIONS. For example:

    configure_cnt0(L1I_CACHE_REFILL, 0xfffffff); 
    */
    configure_cnt2(L1I_CACHE_REFILL, 0xffffff00); 
    configure_cnt1(L1D_CACHE_REFILL, 0xfffffff0); 
}

void notified(sel4cp_channel ch) {
    if (ch == 1) {
        /* WIP: Need to ensure that the shared mem isn't updated by the client before 
        we can get to this section */
        // pmu_config_args_t *config = (pmu_config_args_t *) profiler_control;
        // if (config->notif_opt == PROFILER_START) {
        //     config->notif_opt = PROFILER_READY;
        //     // Notfication to start PMU
        sel4cp_dbg_puts("Starting PMU\n");
        configure_clkcnt(CYCLE_COUNTER_PERIOD);
        resume_cnt();
    //     } else if (config->notif_opt == PROFILER_STOP) {
    //         config->notif_opt = PROFILER_READY;
    //         // Notification to stop PMU
    //         halt_cnt();
    //         // purge any structures left in the array
    //         sel4cp_notify(CLIENT_CH);
    //     } else if (config->notif_opt == PROFILER_CONFIGURE) {
    //         config->notif_opt = PROFILER_READY;
    //         user_pmu_configure(*config);

    //     }
    } else if (ch == 2) {
        sel4cp_dbg_puts("Halting pmu\n");
        halt_cnt();
    }

}

void fault(sel4cp_id id, sel4cp_msginfo msginfo) {
    size_t label = sel4cp_msginfo_get_label(msginfo);
    if (label == seL4_Fault_PMUEvent) {
        uint64_t pc = sel4cp_mr_get(0);
        uint32_t ccnt_lower = sel4cp_mr_get(1);
        uint32_t ccnt_upper = sel4cp_mr_get(2);
        uint32_t pmovsr = sel4cp_mr_get(3);
        uint64_t time = ((uint64_t) ccnt_upper << 32) | ccnt_lower;
  
        // Only add a snapshot if the counter we are sampling on is in the interrupt flag
        // @kwinter Change this to deal with new counter definitions
        if (pmovsr & (IRQ_CYCLE_COUNTER << 31) ||
            pmovsr & (IRQ_COUNTER0 << 0) ||
            pmovsr & (IRQ_COUNTER1 << 1) ||
            pmovsr & (IRQ_COUNTER2 << 2) ||
            pmovsr & (IRQ_COUNTER3 << 3) ||
            pmovsr & (IRQ_COUNTER4 << 4) ||
            pmovsr & (IRQ_COUNTER5 << 5)) {
            add_snapshot(id, time, pc);
        }

        reset_cnt(pmovsr);

        // Resume counters
        resume_cnt();
    }

    sel4cp_fault_reply(sel4cp_msginfo_new(0, 0));

}