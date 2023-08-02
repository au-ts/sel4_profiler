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

uintptr_t uart_base;
uintptr_t profiler_control;

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_avail;
uintptr_t profiler_mem;

ring_handle_t profiler_ring;

/* This global bit string allows us to keep track of what event counters
are being used. We use this bit string when enabling/disabling the PMU */
uint32_t active_counters = (BIT(31)); 

/* Enable the cycle counter, with initial value 0 */
static void enable_cycle_counter() {
	uint64_t init_cnt = 0;
    uint64_t val;

	asm volatile("mrs %0, pmcr_el0" : "=r" (val));

	val |= BIT(2);

    asm volatile("isb; msr pmcr_el0, %0" : : "r" (val));

    asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));

}

static void enable_event_counters() {
    uint64_t val;
    asm volatile("mrs %0, pmcr_el0" : "=r" (val));

	val |= BIT(0);

    asm volatile("isb; msr pmcr_el0, %0" : : "r" (val));
}

void flush_profiler_ring() {
    int size = ring_size(&profiler_ring.used_ring);
    printf_("This is the size of the used ring: %d\n", size);

    uintptr_t buffer = 0;
    unsigned int buffer_len = 0;
    void *cookie;

    while (!dequeue_used(&profiler_ring, &buffer, &buffer_len, &cookie)) {
        perf_sample_t *temp_sample = (perf_sample_t *) buffer;
        printf_("IP: %p\n", temp_sample->ip);
        printf_("PD_ID: %lu\n", temp_sample->id);
        printf_("TIME: %lu\n", temp_sample->time);
        
        int ret = enqueue_avail(&profiler_ring, buffer, buffer_len, &cookie);
        if (ret != 0) {
            sel4cp_dbg_puts("Enqueue in the profiler ring avail failed\n");
        }
    }
}

/* Prints a snapshot of the PMU, as well as the PC of the TCB that has been set */
void print_snapshot(pmu_snapshot_t snapshot) {
    printf_("This is the current cycle counter: %lu\n", snapshot.clock);
    printf_("This is the current program counter: %p\n", snapshot.pc);
    printf_("This is the current event counter 0: %d\n", snapshot.cnt1);
    printf_("This is the current event counter 1: %d\n", snapshot.cnt2);
    printf_("This is the current event counter 2: %d\n", snapshot.cnt3);
    printf_("This is the current event counter 3: %d\n", snapshot.cnt4);
    printf_("This is the current event counter 4: %d\n", snapshot.cnt5);
    printf_("This is the current event counter 5: %d\n", snapshot.cnt6);
}

// void Arch_userStackTrace(tcb_t *tptr)
// {
//     cap_t threadRoot;
//     vspace_root_t *vspaceRoot;
//     word_t sp;

//     threadRoot = TCB_PTR_CTE_PTR(tptr, tcbVTable)->cap;

//     /* lookup the vspace root */
//     if (cap_get_capType(threadRoot) != cap_vtable_root_cap) {
//         printf("Invalid vspace\n");
//         return;
//     }

//     vspaceRoot = cap_vtable_root_get_basePtr(threadRoot);
//     sp = getRegister(tptr, SP_EL0);

//     /* check for alignment so we don't have to worry about accessing
//      * words that might be on two different pages */
//     if (!IS_ALIGNED(sp, seL4_WordSizeBits)) {
//         printf("SP not aligned\n");
//         return;
//     }

//     for (unsigned int i = 0; i < 20; i++) {
//         word_t address = sp + (i * sizeof(word_t));
//         readWordFromVSpace_ret_t result = readWordFromVSpace(vspaceRoot,
//                                                              address);
//         if (result.status == EXCEPTION_NONE) {
//             printf("0x%"SEL4_PRIx_word": 0x%"SEL4_PRIx_word"\n",
//                    address, result.value);
//         } else {
//             printf("0x%"SEL4_PRIx_word": INVALID\n", address);
//         }
//     }
// }

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_snapshot() {
    // This will be added to the raw data section
    pmu_snapshot_t new_entry;
    asm volatile("isb; mrs %0, pmccntr_el0" : "=r" (new_entry.clock));
    asm volatile("isb; mrs %0, pmevcntr0_el0" : "=r" (new_entry.cnt1));
    asm volatile("isb; mrs %0, pmevcntr1_el0" : "=r" (new_entry.cnt2));
    asm volatile("isb; mrs %0, pmevcntr2_el0" : "=r" (new_entry.cnt3));
    asm volatile("isb; mrs %0, pmevcntr3_el0" : "=r" (new_entry.cnt4));
    asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (new_entry.cnt5));
    asm volatile("isb; mrs %0, pmevcntr4_el0" : "=r" (new_entry.cnt5));
    
    seL4_UserContext regs;
    seL4_Error err = seL4_TCB_ReadRegisters(BASE_TCB_CAP + PD_ID, false, 0, SEL4_USER_CONTEXT_SIZE, &regs);
    if (err) {
        new_entry.pc = 0;
    } else {
        new_entry.pc = regs.pc;
    }

    uintptr_t buffer = 0;
    unsigned int buffer_len = 0;
    void * cookie = 0;

    int ret = dequeue_avail(&profiler_ring, &buffer, &buffer_len, &cookie);

    perf_sample_t *temp_sample = (perf_sample_t *) buffer;

    temp_sample->ip = regs.pc;
    temp_sample->pid = PD_ID;
    temp_sample->tid = PD_ID;
    temp_sample->time = sys_now(); /* TODO - Add in timer driver support */
    temp_sample->addr = 0;
    temp_sample->id = 0;
    temp_sample->stream_id = 0;
    temp_sample->cpu = 0;
    temp_sample->period = CYCLE_COUNTER_PERIOD;
    temp_sample->values = 0;
    temp_sample->data = &new_entry;

    // Save the call stack of the interrupting TCB
    // @kwinter Talk to Ivan about how we can distinguish between which TCB is running, and
    // how we can get the correct ip/stack. Also need to somehow do this for kernel, 
    // but I don't think we will have sufficient access here in userspace. 

    uintptr_t fp = regs.x29;
    uintptr_t lr = regs.x30;
    uintptr_t sp = regs.sp;

    printf_("This is the frame pointer of the faulting TCB: %p\n", fp);
    printf_("This is the LR of the faulting TCB: %p\n", lr);
    printf_("This is the SP of the faulting TCB: %p\n", sp);

    ret = enqueue_used(&profiler_ring, buffer, buffer_len, &cookie);
}



/* Check if the PMU has overflowed */
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
    asm volatile("msr pmcntenset_el0, %0" :: "r"(0 << 31));
}

/* Resume the PMU */
void resume_cnt() {
    asm volatile("msr pmcntenset_el0, %0" :: "r" (active_counters));
}

void reset_cycle_cnt() {
    uint64_t init_cnt = 0;

    if (IRQ_CYCLE_COUNTER == 1) {
        init_cnt = 0xffffffffffffffff - CYCLE_COUNTER_PERIOD;
    }

    asm volatile("msr pmccntr_el0, %0" : : "r" (init_cnt));
}

/* Reset the cycle counter to the sampling period. This needs to be changed
to allow sampling on other event counters. */
void reset_cnt(uint32_t interrupt_flags) {
    // Go through all of the interrupt flags, and reset the appropriate counters to
    // the appropriate values. 
    if (interrupt_flags & BIT(0)) {
        printf_("Counter 0 overflowed\n");
        uint32_t val = 0;
        if (IRQ_COUNTER0 == 1) {
            val = 0xffffffff - COUNTER0_PERIOD;
        }
        asm volatile("msr pmevcntr0_el0, %0" : : "r" (val));
    } 

    if (interrupt_flags & (BIT(1))) {
        printf_("Counter 1 overflowed\n");
        uint32_t val = 0;
        if (IRQ_COUNTER1 == 1) {
            val = 0xffffffff - COUNTER1_PERIOD;
        }
        asm volatile("msr pmevcntr1_el0, %0" : : "r" (val));
    } 
    if (interrupt_flags & (BIT(2))) {
        printf_("counter 2 overflowed\n");
        uint32_t val = 0;
        if (IRQ_COUNTER2 == 1) {
            val = 0xffffffff - COUNTER2_PERIOD;
        }
        asm volatile("msr pmevcntr2_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(3))) {
        printf_("counter 3 overflowed\n");

        uint32_t val = 0;
        if (IRQ_COUNTER3 == 1) {
            val = 0xffffffff - COUNTER3_PERIOD;
        }
        asm volatile("msr pmevcntr3_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(4))) {
        printf_("counter 4 overflowed\n");

        uint32_t val = 0;
        if (IRQ_COUNTER4 == 1) {
            val = 0xffffffff - COUNTER4_PERIOD;
        }
        asm volatile("msr pmevcntr4_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(5))) {
        printf_("counter 3 overflowed\n");

        uint32_t val = 0;
        if (IRQ_COUNTER5 == 1) {
            val = 0xffffffff - COUNTER5_PERIOD;
        }
        asm volatile("msr pmevcntr5_el0, %0" : : "r" (val));
    }

    if (interrupt_flags & (BIT(31))) {
        printf_("Cycle counter overflowed\n");
        reset_cycle_cnt();
    }
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
    printf_("Finished configuring, active bit string: %u\n", active_counters);
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
            printf_("Starting PMU\n");
            resume_cnt();
            break;
        case PROFILER_STOP:
            printf_("Stopping the PMU\n");
            halt_cnt();
            // Also want to flush out anything that may be left in the ring buffer
            break;
        case PROFILER_CONFIGURE:
            printf_("Configuring the PMU\n");
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
            printf_("This is the re-constructed value: %ld\n", args.reg_val);
            user_pmu_configure(args);
            break;
        default:
            break;
        }
    }
}

void init () {
    int *prof_cnt = (int *) profiler_control;

    *prof_cnt = 0;

    // Init the record buffers
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_avail, (ring_buffer_t *) profiler_ring_used, NULL, 0);
    init_serial();
    gpt_init();
    
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        int ret = enqueue_avail(&profiler_ring, profiler_mem + (i * sizeof(perf_sample_t)), sizeof(perf_sample_t), NULL);
        if (ret != 0) {
            printf_("This is the number of buffers we were able to enqueue: %d\n", i);
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

    enable_event_counters();
    configure_cnt2(L1I_CACHE_REFILL, 0xffffff00); 
    configure_cnt1(L1D_CACHE_REFILL, 0xfffffff0); 

    // Notifying our dummy program to start running. This is just an empty infinite loop.
    sel4cp_notify(5);
}

void notified(sel4cp_channel ch) {
    // Channel 0 is the interrupt channel for the PMU.
    if (ch == 0) {

        // Halt the PMU
        halt_cnt();

        // Get the reset flags
        uint32_t pmovsr = pmu_getreset_flags();

        // Only add a snapshot if the counter we are sampling on is in the interrupt flag
        // @kwinter Change this to deal with new counter definitions
        if (pmovsr & (IRQ_CYCLE_COUNTER << 31) ||
            pmovsr & (IRQ_COUNTER0 << 0) ||
            pmovsr & (IRQ_COUNTER1 << 1) ||
            pmovsr & (IRQ_COUNTER2 << 2) ||
            pmovsr & (IRQ_COUNTER3 << 3) ||
            pmovsr & (IRQ_COUNTER4 << 4) ||
            pmovsr & (IRQ_COUNTER5 << 5)) {

            printf_("Adding snapshot\n");
            add_snapshot();
        }

        // Check if an overflow has occured
        if(pmu_has_overflowed(pmovsr)) {
            printf_("PMU has overflowed\n");
        } else {
            printf_("PMU hasn't overflowed\n");
        }
        
        // Reset any counters that overflowed.

        // @kwinter Need a way to count how many times a certain counter has overflowed, if we 
        // are not sampling on it. Add this to the raw data section of the perf sample.
        reset_cnt(pmovsr);

        // Resume counters
        resume_cnt();

        // Ack the irq
        sel4cp_irq_ack(ch);
    } else if (ch == 5) {
        /* WIP: Need to ensure that the shared mem isn't updated by the client before 
        we can get to this section */
        pmu_config_args_t *config = (pmu_config_args_t *) profiler_control;
        if (config->notif_opt == PROFILER_START) {
            config->notif_opt = PROFILER_READY;
            printf_("Starting the PMU\n");
            // Notfication to start PMU
            resume_cnt();
        } else if (config->notif_opt == PROFILER_STOP) {
            config->notif_opt = PROFILER_READY;
            printf_("Halting the PMU\n");
            // Notification to stop PMU
            halt_cnt();
            // purge any structures left in the array
            flush_profiler_ring();
        } else if (config->notif_opt == PROFILER_CONFIGURE) {
            config->notif_opt = PROFILER_READY;
            user_pmu_configure(*config);

        }
    } else if (ch == 1) {
        // This is the irq for the timer, divert to the timer_irq() function
        timer_irq(ch);
    }

}