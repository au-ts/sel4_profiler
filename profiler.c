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

/* State of profiler */
int profiler_state;

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
void add_sample(microkit_id id, uint32_t time, uint64_t pc, uint32_t pmovsr, uint64_t *cc) {    
    uintptr_t buffer = 0;
    unsigned int buffer_len = 0;
    void * cookie = 0;

    int ret = dequeue_free(&profiler_ring, &buffer, &buffer_len, &cookie);
    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts("Failed to dequeue from profiler free ring\n");
        return;
    }
    pmu_sample_t *temp_sample = (pmu_sample_t *) buffer;
    
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
    for (int i = 0; i < MAX_CALL_DEPTH; i++) {
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
        reset_cnt(pmovsr);
        halt_cnt();
        microkit_notify(CLIENT_CH);
    } else {
        reset_cnt(pmovsr);
        resume_cnt();
    }
}

/* Initial user PMU configure interface. This is called during a PPC. With the root/child abstraction, PPC's do not currently work. */
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
protected(microkit_channel ch, microkit_msginfo msginfo)
{
    // This is deprecated for now. Using a shared memory region between process and profiler to control/configure
    // For now we should only be recieving on channel 5
    if (ch == 5) {
        // For now, we should have only 1 message 
        uint32_t config = microkit_mr_get(0);

        switch (config)
        {
        case PROFILER_START:
            // Start PMU
            resume_cnt();
            break;
        case PROFILER_STOP:
            halt_cnt();
            // Also want to flush out anything that may be left in the ring buffer
            // Purge buffers if any
            microkit_notify(CLIENT_HALT);
            break;
        case PROFILER_CONFIGURE:
            {
            /* Example layout of what a call to profile configure should look like (Based on the perfmon2 spec):
                microkit_mr_set(0, PROFILER_CONFIGURE);
                microkit_mr_set(1, REG_NUM);
                microkit_mr_set(2, EVENT NUM);
                microkit_mr_set(3, FLAGS(empty for now));
                microkit_mr_set(4, VAL_UPPER);
                microkit_mr_set(5, VAL_LOWER);
            These message registers are then unpacted here and applied to the PMU state.     
            */

            pmu_config_args_t args;
            args.reg_num = microkit_mr_get(1);
            args.reg_event = microkit_mr_get(2);
            args.reg_flags = microkit_mr_get(3);
            uint32_t top = microkit_mr_get(4);
            uint32_t bottom = microkit_mr_get(5);
            args.reg_val = ((uint64_t) top << 32) | bottom;
            user_pmu_configure(args);
            break;
            }
        default:
            break;
        }
    }
    return microkit_msginfo_new(0, 0);
}

void init () {
    int *prof_cnt = (int *) profiler_control;

    *prof_cnt = 0;

    // Init the record buffers
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 0, 512, 512);
    
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        int ret = enqueue_free(&profiler_ring, profiler_mem + (i * sizeof(perf_sample_t)), sizeof(perf_sample_t), NULL);
        
        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to populate buffers for the perf record ring buffer\n");
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

    // Make sure that the PMU is not running until we start
    halt_cnt();

    // Set the profiler state to init
    profiler_state = PROF_INIT;
}

void notified(microkit_channel ch) {
    if (ch == 10) {
        microkit_dbg_puts("Starting PMU\n");
        // Set the profiler state to start
        profiler_state = PROF_START;
        configure_clkcnt(CYCLE_COUNTER_PERIOD);
        resume_cnt();
    } else if (ch == 20) {
        microkit_dbg_puts("Halting PMU\n");
        // Set the profiler state to halt
        profiler_state = PROF_HALT;
        halt_cnt();
        // Purge any buffers that may be leftover
        microkit_notify(CLIENT_CH);
    } else if (ch == 30) {
        // Only resume if profiler state is in 'START' state
        if (profiler_state == PROF_START) {
            microkit_dbg_puts("Resuming PMU\n");
            resume_cnt();
        }
    }
}
static char *
data_abort_dfsc_to_string(uintptr_t dfsc)
{
    switch(dfsc) {
        case 0x00: return "address size fault, level 0";
        case 0x01: return "address size fault, level 1";
        case 0x02: return "address size fault, level 2";
        case 0x03: return "address size fault, level 3";
        case 0x04: return "translation fault, level 0";
        case 0x05: return "translation fault, level 1";
        case 0x06: return "translation fault, level 2";
        case 0x07: return "translation fault, level 3";
        case 0x09: return "access flag fault, level 1";
        case 0x0a: return "access flag fault, level 2";
        case 0x0b: return "access flag fault, level 3";
        case 0x0d: return "permission fault, level 1";
        case 0x0e: return "permission fault, level 2";
        case 0x0f: return "permission fault, level 3";
        case 0x10: return "synchronuos external abort";
        case 0x11: return "synchronous tag check fault";
        case 0x14: return "synchronous external abort, level 0";
        case 0x15: return "synchronous external abort, level 1";
        case 0x16: return "synchronous external abort, level 2";
        case 0x17: return "synchronous external abort, level 3";
        case 0x18: return "syncrhonous partity or ECC error";
        case 0x1c: return "syncrhonous partity or ECC error, level 0";
        case 0x1d: return "syncrhonous partity or ECC error, level 1";
        case 0x1e: return "syncrhonous partity or ECC error, level 2";
        case 0x1f: return "syncrhonous partity or ECC error, level 3";
        case 0x21: return "alignment fault";
        case 0x30: return "tlb conflict abort";
        case 0x31: return "unsupported atomic hardware update fault";
    }
    return "<unexpected DFSC>";
}
static char *
ec_to_string(uintptr_t ec)
{
    switch (ec) {
        case 0: return "Unknown reason";
        case 1: return "Trapped WFI or WFE instruction execution";
        case 3: return "Trapped MCR or MRC access with (coproc==0b1111) this is not reported using EC 0b000000";
        case 4: return "Trapped MCRR or MRRC access with (coproc==0b1111) this is not reported using EC 0b000000";
        case 5: return "Trapped MCR or MRC access with (coproc==0b1110)";
        case 6: return "Trapped LDC or STC access";
        case 7: return "Access to SVC, Advanced SIMD or floating-point functionality trapped";
        case 12: return "Trapped MRRC access with (coproc==0b1110)";
        case 13: return "Branch Target Exception";
        case 17: return "SVC instruction execution in AArch32 state";
        case 21: return "SVC instruction execution in AArch64 state";
        case 24: return "Trapped MSR, MRS or System instruction exuection in AArch64 state, this is not reported using EC 0xb000000, 0b000001 or 0b000111";
        case 25: return "Access to SVE functionality trapped";
        case 28: return "Exception from a Pointer Authentication instruction authentication failure";
        case 32: return "Instruction Abort from a lower Exception level";
        case 33: return "Instruction Abort taken without a change in Exception level";
        case 34: return "PC alignment fault exception";
        case 36: return "Data Abort from a lower Exception level";
        case 37: return "Data Abort taken without a change in Exception level";
        case 38: return "SP alignment faultr exception";
        case 40: return "Trapped floating-point exception taken from AArch32 state";
        case 44: return "Trapped floating-point exception taken from AArch64 state";
        case 47: return "SError interrupt";
        case 48: return "Breakpoint exception from a lower Exception level";
        case 49: return "Breakpoint exception taken without a change in Exception level";
        case 50: return "Software Step exception from a lower Exception level";
        case 51: return "Software Step exception taken without a change in Exception level";
        case 52: return "Watchpoint exception from a lower Exception level";
        case 53: return "Watchpoint exception taken without a change in Exception level";
        case 56: return "BKPT instruction execution in AArch32 state";
        case 60: return "BRK instruction execution in AArch64 state";
    }
    return "<invalid EC>";
}
void fault(microkit_id id, microkit_msginfo msginfo) {
    microkit_dbg_puts("recv fault in prof: ");
    puthex64(id);
    microkit_dbg_puts("\n");
    size_t label = microkit_msginfo_get_label(msginfo);
    if (label == seL4_Fault_PMUEvent) {
        microkit_dbg_puts("recv pmu fault\n");
        uint64_t pc = microkit_mr_get(0);
        uint32_t ccnt_lower = microkit_mr_get(1);
        uint32_t ccnt_upper = microkit_mr_get(2);
        uint32_t pmovsr = microkit_mr_get(3);
        uint64_t cc[4] = {0,0,0,0};
        cc[0] = microkit_mr_get(4);
        cc[1] = microkit_mr_get(5);
        cc[2] = microkit_mr_get(6);
        cc[3] = microkit_mr_get(7);

        uint64_t time = ((uint64_t) ccnt_upper << 32) | ccnt_lower;
        // profiler_handle_fault(pc, ccnt_lower, ...);
  
        // Only add a snapshot if the counter we are sampling on is in the interrupt flag
        // @kwinter Change this to deal with new counter definitions
        if (pmovsr & (IRQ_CYCLE_COUNTER << 31) ||
            pmovsr & (IRQ_COUNTER0 << 0) ||
            pmovsr & (IRQ_COUNTER1 << 1) ||
            pmovsr & (IRQ_COUNTER2 << 2) ||
            pmovsr & (IRQ_COUNTER3 << 3) ||
            pmovsr & (IRQ_COUNTER4 << 4) ||
            pmovsr & (IRQ_COUNTER5 << 5)) {
            add_sample(id, time, pc, pmovsr, cc);
        } else {
            reset_cnt(pmovsr);
            resume_cnt();
        }
    } else if (label == seL4_Fault_VMFault) {
        seL4_Word ip = seL4_GetMR(seL4_VMFault_IP);
        seL4_Word fault_addr = seL4_GetMR(seL4_VMFault_Addr);
        seL4_Word is_instruction = seL4_GetMR(seL4_VMFault_PrefetchFault);
        seL4_Word fsr = seL4_GetMR(seL4_VMFault_FSR);
        seL4_Word ec = fsr >> 26;
        seL4_Word il = fsr >> 25 & 1;
        seL4_Word iss = fsr & 0x1ffffffUL;
        microkit_dbg_puts("MON|ERROR: VMFault: ip=");
        puthex64(ip);
        microkit_dbg_puts("  fault_addr=");
        puthex64(fault_addr);
        microkit_dbg_puts("  fsr=");
        puthex64(fsr);
        microkit_dbg_puts("  ");
        microkit_dbg_puts(is_instruction ? "(instruction fault)" : "(data fault)");
        microkit_dbg_puts("\n");
        microkit_dbg_puts("MON|ERROR:    ec: ");
        puthex64(ec);
        microkit_dbg_puts("  ");
        microkit_dbg_puts(ec_to_string(ec));
        microkit_dbg_puts("   il: ");
        microkit_dbg_puts(il ? "1" : "0");
        microkit_dbg_puts("   iss: ");
        puthex64(iss);
        microkit_dbg_puts("\n");

        if (ec == 0x24) {
            /* FIXME: Note, this is not a complete decoding of the fault! Just some of the more
                common fields!
            */
            seL4_Word dfsc = iss & 0x3f;
            bool ea = (iss >> 9) & 1;
            bool cm = (iss >> 8) & 1;
            bool s1ptw = (iss >> 7) & 1;
            bool wnr = (iss >> 6) & 1;
            microkit_dbg_puts("MON|ERROR:    dfsc = ");
            microkit_dbg_puts(data_abort_dfsc_to_string(dfsc));
            microkit_dbg_puts(" (");
            puthex64(dfsc);
            microkit_dbg_puts(")");
            if (ea) {
                microkit_dbg_puts(" -- external abort");
            }
            if (cm) {
                microkit_dbg_puts(" -- cache maint");
            }
            if (s1ptw) {
                microkit_dbg_puts(" -- stage 2 fault for stage 1 page table walk");
            }
            if (wnr) {
                microkit_dbg_puts(" -- write not read");
            }
            microkit_dbg_puts("\n");
        }
        while (1);
    } else if (label == seL4_Fault_CapFault) {
        microkit_dbg_puts("Cap fault\n");
    } else if (label == seL4_Fault_UnknownSyscall) {
        microkit_dbg_puts("Unknown syscall\n");
    } else if (label == seL4_Fault_UserException) {
        microkit_dbg_puts("User Exception\n");
    }  else if (label == seL4_Fault_NullFault) {
        microkit_dbg_puts("Null fault\n");
    }

    microkit_fault_reply(microkit_msginfo_new(0, 0));

}