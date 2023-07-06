#pragma once
#include <stdint.h>

struct pmu_snapshot {
    uint64_t clock;
    uint32_t cnt1;
    uint32_t cnt2;
    uint32_t cnt3;
    uint32_t cnt4;
    uint32_t cnt5;
};

typedef struct pmu_snapshot pmu_snapshot_t;
