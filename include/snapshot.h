#pragma once
#include <stdint.h>

struct pmu_snapshot {
    // We only really need to keep track of the PC for now
    uint64_t pc;
    uint64_t clock;
    uint32_t cnt1;
    uint32_t cnt2;
    uint32_t cnt3;
    uint32_t cnt4;
    uint32_t cnt5;
    uint32_t cnt6;
};

typedef struct pmu_snapshot pmu_snapshot_t;
