/* BASIC TIMER INTERFACE TO GET CURRENT SYSTEM TIME*/

#include <stdint.h>
#include "timer.h"

uintptr_t gpt_regs;

static volatile uint32_t *gpt;

static uint32_t overflow_count;

#define CR 0
#define PR 1
#define SR 2
#define IR 3
#define OCR1 4
#define OCR2 5
#define OCR3 6
#define ICR1 7
#define ICR2 8
#define CNT 9

#define NS_IN_MS 1000000ULL

int timers_intialised = 0;

static uint64_t get_ticks(void) {
    /* FIXME: If an overflow interrupt happens in the middle here we are in trouble */
    uint64_t overflow = overflow_count;
    uint32_t sr1 = gpt[SR];
    uint32_t cnt = gpt[CNT];
    uint32_t sr2 = gpt[SR];
    if ((sr2 & (1 << 5)) && (!(sr1 & (1 << 5)))) {
        /* rolled-over during - 64-bit time must be the overflow */
        cnt = gpt[CNT];
        overflow++;
    }
    return (overflow << 32) | cnt;
}

void gpt_init(void) {
    gpt = (volatile uint32_t *) gpt_regs;
    uint32_t cr = (
        (1 << 9) | // Free run mode
        (1 << 6) | // Peripheral clocks
        (1)
    );

    gpt[CR] = cr;

    timers_intialised = 1;
}

uint64_t sys_now(void) {
    if (!timers_intialised) {
        return 0;
    } else {
        return get_ticks();
    }
}

void timer_irq(sel4cp_channel ch) {
    uint32_t sr = gpt[SR];
    gpt[SR] = sr;

    if (sr & (1 << 5)) {
        overflow_count++;
    }
}