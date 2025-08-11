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

void indirection3(void) {
    sddf_dprintf("And one last one!\n");
    big_loop();
}

void indirection2(void) {
    sddf_dprintf("And another one\n");
    indirection3();
}

void indirection1(void) {
    sddf_dprintf("This is another layer of indirection\n");
    indirection2();
}

void init() {
}

void notified (microkit_channel ch) {
    sddf_dprintf("in dummy prog 2\n");
    indirection1();
}