#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <sddf/util/printf.h>
#include "util.h"


void big_loop() {
    for (int i = 0; i < 9999999999999; i++) {
        i = i;
    }
}

void init() {
    #ifdef CONFIG_PROFILER_ENABLE
    seL4_ProfilerRegisterThread(2);
    #endif
}

void notified (microkit_channel ch) {
    sddf_dprintf("in dummy prog 2\n");
    big_loop();
}