#pragma once

/* ----- THE FOLLOWING DEFINES ARE FOR CONFIGURING THE PROFILER ------*/

// TODO: NEED TO HAVE A BETTER WAY OF INJECTING THE MAPPINGS FROM THE SYSTEM DESCRIPTION

/*
    This is the string that maintains the mapping between profiler id's and elf names.
    The profiler id is the value with which you make the the seL4_ProfilerRegisterThread(id) syscall.

    Please change this string when adding processes to profile. Use the following structure:

    "<elf_name>: <profiling_id>\n
    <elf_name>: <profiling_id"

    NOTE: Seperate pairs with new lines.
*/  

#define MAPPINGS_STR "echo.elf: 52"

/* For each counter, set IRQ to 1 to sample on counter, or 0 to not.

If 1 is set, ALSO set the appropriate sampling period. */

#define IRQ_CYCLE_COUNTER 1
#define CYCLE_COUNTER_PERIOD 1200000

#define IRQ_COUNTER0 0
#define COUNTER0_PERIOD 0

#define IRQ_COUNTER1 0
#define COUNTER1_PERIOD 120

#define IRQ_COUNTER2 0
#define COUNTER2_PERIOD 1200

#define IRQ_COUNTER3 0
#define COUNTER3_PERIOD 0

#define IRQ_COUNTER4 0
#define COUNTER4_PERIOD 0

#define IRQ_COUNTER5 0
#define COUNTER5_PERIOD 0
