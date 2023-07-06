#include <stdbool.h>
#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "printf.h"

#define PROFILER_CH 5

void endless_loop() {
    sel4cp_dbg_puts("We are going to loop in the dummy prog\n");
    uint64_t i = 0;
    int ticker = 0;
    while(1) {
        if (i == 999999999) {
            sel4cp_dbg_puts("Still looping\n");
            i = 0;
            ticker++;
        } else if (ticker == 10) {
            break;
        }
        
        i += 1;
    }
}

void init() {
    // Nothing to init
}

void notified (sel4cp_channel ch) {
    // PPcall the profiler to start the tracking
    sel4cp_mr_set(0, PROFILER_START);
    sel4cp_ppcall(PROFILER_CH, sel4cp_msginfo_new(0, 1));
    endless_loop();
    // PPcall the profiler to stop the tracking
    sel4cp_mr_set(0, PROFILER_STOP);
    sel4cp_ppcall(PROFILER_CH, sel4cp_msginfo_new(0, 1));
}