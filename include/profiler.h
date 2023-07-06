#pragma once

#define BIT(nr) (1UL << (nr))
// CHANGE THIS VALUE TO CHANGE THE FREQUENCY OF SAMPLING
#define SAMPLING_PERIOD 1200000000

#define ARMV8_PMEVTYPER_EVTCOUNT_MASK 0x3ff
#define ARMV8_PMCNTENSET_EL0_ENABLE (1<<31) /* *< Enable Perf count reg */
#define ARMV8_OVSR_MASK 0xffffffff 

// Flags to pass through Protected Procedure Call to configure PMU
#define PROFILER_START 0
#define PROFILER_STOP 1