#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"

void endless_loop() {
    uint64_t i = 0;
    int ticker = 0;
    while(1) {
        if (i == 999999999) {
            i = 0;
            microkit_notify(6);
            ticker++;
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