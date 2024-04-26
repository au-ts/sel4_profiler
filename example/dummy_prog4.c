#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"

void big_loop() {
    for (int i = 0; i < 9999999999999; i++) {
        i = i;
    }
}

void init() {
    #ifdef CONFIG_PROFILER_ENABLE
    seL4_ProfilerRegisterThread(4);
    #endif
}

void notified (microkit_channel ch) {
    microkit_dbg_puts("in dummy prog 4\n");
    big_loop();
}