#pragma once

#include <sddf/resources/common.h>
#include <stdint.h>

typedef struct profiler_config {
    region_resource_t profiler_ring_used;
    region_resource_t profiler_ring_free;
    region_resource_t profiler_mem;
    uint16_t num_cli;
    uint8_t ch;
} profiler_config_t;
