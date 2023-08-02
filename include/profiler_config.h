#pragma once

/* ----- THE FOLLOWING DEFINES ARE FOR CONFIGURING THE PROFILER ------*/

/* CHANGE THIS VALUE TO CHANGE WHAT PD TO TRACK (WE NEED TO ACCESS THE TCB OF
A PD TO TRACK PC) */
#define PD_ID 0


/* For each counter, set IRQ to 1 to sample on counter, or 0 to not.

If 1 is set, ALSO set the appropriate sampling period. */

#define IRQ_CYCLE_COUNTER 1
#define CYCLE_COUNTER_PERIOD 120000000000

#define IRQ_COUNTER0 0
#define COUNTER0_PERIOD 0

#define IRQ_COUNTER1 1
#define COUNTER1_PERIOD 120

#define IRQ_COUNTER2 1
#define COUNTER2_PERIOD 1200

#define IRQ_COUNTER3 0
#define COUNTER3_PERIOD 0

#define IRQ_COUNTER4 0
#define COUNTER4_PERIOD 0

#define IRQ_COUNTER5 0
#define COUNTER5_PERIOD 0
