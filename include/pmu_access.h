/* Header file for a basic PMU access library, wrapping around the raw seL4
invocations to the PMU Access Control capability. */

/* TODO: Hide architecture in here. Wrap ARM and RISCV around ifdefs*/
#pragma once

#include <stdint.h>
#include <sel4/sel4.h>

/* Get number of hardware counters availabel for use */
static inline uint32_t libpmu_get_num_counters(seL4_Word pmu_cap, uint32_t counter)
{
    seL4_ARM_PMUControl_NumCounters_t ret = seL4_ARM_PMUControl_NumCounters(pmu_cap);
    if (!ret.error) {
        return ret.num_counters;
    } else {
        return ret.error;
    }
}

/* Read event counter */
static inline uint32_t libpmu_read_event_counter(seL4_Word pmu_cap, uint32_t counter)
{
    seL4_ARM_PMUControl_ReadEventCounter_t ret = seL4_ARM_PMUControl_ReadEventCounter(pmu_cap, counter);
    if (!ret.error) {
        return (uint32_t) ret.counter_value;
    } else {
        return ret.error;
    }
}

/* Write event counter */
static inline seL4_Error libpmu_write_event_counter(seL4_Word pmu_cap, uint32_t counter, uint32_t cnt_val, uint32_t event)
{
    return seL4_ARM_PMUControl_WriteEventCounter(pmu_cap, counter, cnt_val, event);
}


/* Read the cycle counter */
static inline uint64_t libpmu_read_cycle_counter(seL4_Word pmu_cap)
{
    seL4_ARM_PMUControl_ReadCycleCounter_t ret = seL4_ARM_PMUControl_ReadCycleCounter(pmu_cap);
    if (!ret.error) {
        return (uint64_t) ret.counter_value;
    } else {
        return (uint64_t) ret.error;
    }
}

/* Write the cycle counter */
static inline seL4_Error libpmu_write_cycle_counter(seL4_Word pmu_cap, uint64_t counter_value)
{
    return seL4_ARM_PMUControl_WriteCycleCounter(pmu_cap, counter_value);
}

/* Start all PMU counters */
static inline seL4_Error libpmu_start_counters(seL4_Word pmu_cap)
{
    return seL4_ARM_PMUControl_CounterControl(pmu_cap, 1);
}

/* Stop all PMU counters */
static inline seL4_Error libpmu_stop_counters(seL4_Word pmu_cap)
{
    return seL4_ARM_PMUControl_CounterControl(pmu_cap, 0);
}

/* Read interrupt value */
static inline uint32_t libpmu_read_int_val(seL4_Word pmu_cap)
{
    seL4_ARM_PMUControl_InterruptValue_t ret = seL4_ARM_PMUControl_ReadInterruptValue(pmu_cap);
    if (!ret.error) {
        return ret.interrupt_val;
    } else {
        return ret.error;
    }
}

/* Write interrupt value */
static inline seL4_Error libpmu_write_int_val(seL4_Word pmu_cap, uint32_t int_val)
{
    return seL4_ARM_PMUControl_WriteInterruptValue(pmu_cap, int_val);
}

/* Write interrupt control */
static inline seL4_Error libpmu_write_int_ctrl(seL4_Word pmu_cap, uint32_t int_ctrl)
{
    return seL4_ARM_PMUControl_InterruptControl(pmu_cap, int_ctrl);
}