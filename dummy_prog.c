#include <stdbool.h>
#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "printf.h"

#define PROFILER_CH_START 10
#define PROFILER_CH_END 20

uintptr_t profiler_control;

void endless_loop() {
    uint64_t i = 0;
    int ticker = 0;
    while(1) {
        if (i == 999999999) {
            i = 0;
            sel4cp_notify(6);
            ticker++;
        } else if (ticker == 5) {
            break;
        }
        
        i += 1;
    }
}

void init() {
    endless_loop();
}

void notified (sel4cp_channel ch) {
}