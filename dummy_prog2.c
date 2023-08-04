#include <stdbool.h>
#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "printf.h"


uintptr_t profiler_control;

void big_loop() {
    for (int i = 0; i < 9999999999999; i++) {
        i = i;
    }
}

void init() {
    // Nothing to init
}

void notified (sel4cp_channel ch) {
    sel4cp_dbg_puts("in dummy prog 2\n");
    big_loop();
}