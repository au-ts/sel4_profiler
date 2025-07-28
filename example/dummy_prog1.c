#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include "util.h"

void endless_loop() {
    uint64_t i = 0;
    int ticker = 0;
    while(1) {
        if (i == 999999999) {
            i = 0;
            ticker++;
            microkit_notify(1);
        } else if (ticker == 5) {
            break;
        }

        i += 1;
    }
}

void init() {
    #ifdef CONFIG_PROFILER_ENABLE
    seL4_ProfilerRegisterThread(1);
    #endif
    endless_loop();
}

void notified (microkit_channel ch) {
}