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

uintptr_t profiler_control;

void endless_loop() {
    uint64_t i = 0;
    int ticker = 0;
    while(1) {
        if (i == 999999999) {
            i = 0;
            ticker++;
        } else if (ticker == 5) {
            break;
        }
        
        i += 1;
    }
}

void init() {
    // Nothing to init
}

void notified (sel4cp_channel ch) {

    // sel4cp_dbg_puts("In the dummy prog notified\n");
    // // Setup an event register using interface
    // uint64_t reg_val = 500;
    // sel4cp_mr_set(0, PROFILER_CONFIGURE);
    // sel4cp_mr_set(1, EVENT_CTR_0);
    // sel4cp_mr_set(2, L1D_CACHE);
    // sel4cp_mr_set(3, 0);
    // sel4cp_mr_set(4, (reg_val >> 32) & 0xffffffff);
    // sel4cp_mr_set(5, (0xffffffff & reg_val));
    // sel4cp_ppcall(PROFILER_CH, sel4cp_msginfo_new(0, 6));

    // sel4cp_mr_set(0, PROFILER_CONFIGURE);
    // sel4cp_mr_set(1, EVENT_CTR_1);
    // sel4cp_mr_set(2, BR_MIS_PRED);
    // sel4cp_mr_set(3, 0);
    // sel4cp_mr_set(4, (reg_val >> 32) & 0xffffffff);
    // sel4cp_mr_set(5, (0xffffffff & reg_val));
    // sel4cp_ppcall(PROFILER_CH, sel4cp_msginfo_new(0, 6));

    // // PPcall the profiler to start the tracking
    // sel4cp_mr_set(0, PROFILER_START);
    // sel4cp_ppcall(PROFILER_CH, sel4cp_msginfo_new(0, 1));
    pmu_config_args_t *config = (pmu_config_args_t *) profiler_control;
    // config->notif_opt = PROFILER_CONFIGURE;
    // config->reg_num = EVENT_CTR_0;
    // config->reg_event = L1D_CACHE;
    // config->reg_flags = 0;
    // config->reg_val = 500;
    // sel4cp_notify(PROFILER_CH);


    config->notif_opt = PROFILER_START;
    sel4cp_notify(PROFILER_CH);
    endless_loop();

    config->notif_opt = PROFILER_STOP;
    sel4cp_notify(PROFILER_CH);
    // // PPcall the profiler to stop the tracking
    // sel4cp_mr_set(0, PROFILER_STOP);
    // sel4cp_ppcall(PROFILER_CH, sel4cp_msginfo_new(0, 1));
}