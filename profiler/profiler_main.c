#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include <string.h>
#include "profiler.h"
#include "util.h"
#include "serial_server.h"
#include "snapshot.h"
#include "timer.h"
#include "profiler_config.h"
#include "client.h"
#include <sddf/network/shared_ringbuffer.h>

uintptr_t uart_base;

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_free;
uintptr_t profiler_mem;
ring_handle_t profiler_ring;

uintptr_t log_buffer;

// TODO: Move these out of here
#define NUM_PROF_THREADS 1

/* LAY THE THREAD CHANNELS OUT AS FOLLOWS:
    1 - START0
    2 - STOP0
    3 - RECV_SAMPLE0
    4 - START1
    5 - START1
    6 - RECV_SAMPLE1
    .
    .
    .
    n + 0 - START((n/3)+1)
    n + 1 - STOP((n/3)+2)
    n + 2 - RECV_SAMPLE((n/3)+3)
*/

#define START0_CH 1
#define STOP0_CH 2
#define RECV_SAMPLE0_CH 3

/* State of profiler */
int profiler_state;

void resume_threads() {
    microkit_notify(START0_CH);
}

void restart_threads() {
    microkit_notify(RECV_SAMPLE0_CH);
}

void halt_threads() {
    microkit_notify(STOP0_CH);
}

/* Add a snapshot of the cycle and event registers to the array. This array needs to become a ring buffer. */
void add_sample(int thread_id) {
    pmu_sample_t *profLogs = (pmu_sample_t *) log_buffer;
    // Get the current CPU affinity of this thread.
    pmu_sample_t profLog = profLogs[thread_id];

    buff_desc_t buffer;
    int ret = dequeue_free(&profiler_ring, &buffer);
    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts("Failed to dequeue from profiler free ring\n");
        return;
    }

    prof_sample_t *temp_sample = (prof_sample_t *) buffer.phys_or_offset;

    // Find which counter overflowed, and the corresponding period

    temp_sample->ip = profLog.ip;
    temp_sample->pid = profLog.pid;
    temp_sample->time = profLog.time;
    temp_sample->cpu = profLog.cpu;
    // TO-DO: Figure out how we are going to get the period??
    temp_sample->period = 0;
    temp_sample->nr = profLog.nr;
    for (int i = 0; i < SEL4_PROF_MAX_CALL_DEPTH; i++) {
        temp_sample->ips[i] = 0;
        temp_sample->ips[i] = profLog.ips[i];
    }

    ret = enqueue_used(&profiler_ring, buffer);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts("Failed to dequeue from profiler used ring\n");
        return;
    }

    // Notify the client that we need to dump. If we are dumping, do not
    // restart the PMU until we have managed to purge all buffers over the network.
    if (ring_empty(profiler_ring.free_ring)) {
        microkit_notify(CLIENT_PROFILER_CH);
    } else {
        restart_threads();
    }
}

void init () {
    microkit_dbg_puts("Profiler intialising...\n");

    // Init the record buffers
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 512);

    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        buff_desc_t buffer;
        buffer.phys_or_offset = profiler_mem + (i * sizeof(prof_sample_t));
        buffer.len = sizeof(prof_sample_t);
        int ret = enqueue_free(&profiler_ring, buffer);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to populate buffers for the perf record ring buffer\n");
            break;
        }
    }

    #ifdef CONFIG_PROFILER_ENABLE
    int res_buf = seL4_BenchmarkSetLogBuffer(log_buffer);
    if (res_buf) {
        microkit_dbg_puts("Could not set log buffer");
        puthex64(res_buf);
    }
    #endif

    // Set the profiler state to init
    profiler_state = PROF_INIT;
}

void notified(microkit_channel ch) {
    if (ch == 10) {
        // Set the profiler state to start
        profiler_state = PROF_START;
        resume_threads();
    } else if (ch == 20) {
        // Set the profiler state to halt
        profiler_state = PROF_HALT;
        halt_threads();
        // Purge any buffers that may be leftover
        microkit_notify(CLIENT_PROFILER_CH);
    } else if (ch == 30) {
        // Only resume if profiler state is in 'START' state
        if (profiler_state == PROF_START) {
            restart_threads();
        }
    } else if (ch == RECV_SAMPLE0_CH) {
        // Add sample from index 0 of the log buffer
        add_sample(1);
    }
}
