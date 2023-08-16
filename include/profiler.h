#pragma once

#define BIT(nr) (1UL << (nr))


#define SEL4_USER_CONTEXT_SIZE 0x24

#define ARMV8_PMEVTYPER_EVTCOUNT_MASK 0x3ff
#define ARMV8_PMCNTENSET_EL0_ENABLE (1<<31) /* *< Enable Perf count reg */
#define ARMV8_OVSR_MASK 0xffffffff 

// Flags to pass through Protected Procedure Call to configure PMU
#define PROFILER_START 0
#define PROFILER_STOP 1
#define PROFILER_CONFIGURE 2
#define PROFILER_READY 3
// Definitions of counters available on PMU
#define CYCLE_CTR 0
#define EVENT_CTR_0 1
#define EVENT_CTR_1 2
#define EVENT_CTR_2 3
#define EVENT_CTR_3 4
#define EVENT_CTR_4 5
#define EVENT_CTR_5 6

// Event definitions for ARMv8 PMU
#define SW_INCR 0x00                /* Software Increment*/
#define L1I_CACHE_REFILL 0x01       /* L1 Instruction cache refill */
#define L1I_TLB_REFILL 0x02         /* L1 Instruction TLB refill */
#define L1D_CACHE_REFILL 0x03       /* L1 Data cache refill */
#define L1D_CACHE 0x04              /* L1 Data cache access */
#define L1D_TLB_REFILL 0x05         /* L1 Data TLB refill */
#define LD_RETIRED 0x06             /* Instruction architecturally executed, condition check pass - load */
#define ST_RETIRED 0x07             /* Instruction architecturally executed, condition check pass - store */
#define INST_RETIRED 0x08           /* Instruction architecturally executed */
#define EXC_TAKEN 0x09              /* Exception taken */
#define EXC_RETURN 0x0a             /* Exception returned */
#define CID_WRITE_RETIRED 0x0b      /* Change to Context ID retired */
#define PC_WRITE_RETIRED 0x0c       /* Instruction architecturally executed, condition check pass - write to CONTEXTIDR */
#define BR_IMMED_RETIRED 0x0d       /* Instruction architecturally executed, condition check pass - software change of the PC */
#define UNALIGNED_LDST_RETIRED 0x0f /* Instruction architecturally executed, condition check pass, prodcedure return */
#define BR_MIS_PRED 0x10            /* Mispredicted or not predicted branch speculatively executed */
#define CPU_CYCLES 0x11             /* Cycle */
#define BR_PRED 0x12                /* Predictable branch speculatively executed */
#define MEM_ACCESS 0x13             /* L1 Data cache access */
#define L1I_CACHE 0x14              /* L1 Instruction cache access */
#define L1D_CACHE_WB 0x15           /* L1 Data cache Write-back */
#define L2D_CACHE 0x16              /* L2 Data cache access */
#define L2D_CACHE_REFILL 0x17       /* L2 Data cache refill */
#define L2D_CACHE_WB 0x18           /* L2 Data cache write-back */
#define BUS_ACCESS 0x19             /* Bus access */
#define MEMORY_ACCESS 0x1a          /* Local memory error */
#define BUS_CYCLES 0x1d             /* Bus cycle */
#define CHAIN 0x1e                  /* Odd performance counter chain mode */
#define BUS_ACCESS_LD 0x60          /* Bus access - Read */

/* TO-DO: There are more events as defined in the cortex a55 spec. Add these here. */

struct pmu_config_args {
    uint8_t notif_opt; /* This is the kind of operation we want the profiler to carry out. 0 for start profiling, 1 for stop profiling, 2 for configure.*/
    uint16_t reg_num;
    uint16_t reg_event;
    uint32_t reg_flags;
    uint64_t reg_val;
};

typedef struct pmu_config_args pmu_config_args_t;