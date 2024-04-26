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
#include <sddf/util/printf.h>

uintptr_t uart_base;
uintptr_t log_buffer;

uintptr_t profiler_ring_used_t0;
uintptr_t profiler_ring_free_t0;
uintptr_t profiler_mem_t0;
uintptr_t profiler_ring_used_t1;
uintptr_t profiler_ring_free_t1;
uintptr_t profiler_mem_t1;
ring_handle_t profiler_rings[NUM_PROF_THREADS];

uintptr_t prof_cli_ring_used;
uintptr_t prof_cli_ring_free;
uintptr_t prof_cli_mem;
ring_handle_t prof_cli_ring;

bool prof_thread_waiting[NUM_PROF_THREADS] = {false};

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

#define PROF_THREAD0 0
#define PROF_THREAD1 1

/* State of profiler */
int profiler_state;

void resume_threads() {
    for (int i = 0; i < NUM_PROF_THREADS; i++) {
        microkit_ppcall(i, microkit_msginfo_new(PROFILER_START, 0));
    }
}

void restart_threads() {
    for (int i = 0; i < NUM_PROF_THREADS; i++) {
        microkit_ppcall(i, microkit_msginfo_new(PROFILER_RESTART, 0));
    }
}

void halt_threads() {
    for (int i = 0; i < NUM_PROF_THREADS; i++) {
        microkit_ppcall(i, microkit_msginfo_new(PROFILER_STOP, 0));
    }
}

void purge_thread(microkit_channel ch) {
    /* For now, we will copy from the thread to our ring with the cli.
       This is definitely not the optimal solution for now. */
    while (!ring_empty(profiler_rings[ch].used_ring) && !ring_empty(prof_cli_ring.free_ring)) {
        buff_desc_t thread_buffer;
        buff_desc_t cli_buffer;

        int ret = dequeue_used(&profiler_rings[ch], &thread_buffer);
        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to dequeue from thread used rings\n");
            break;
        }

        ret = dequeue_free(&prof_cli_ring, &cli_buffer);
        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to dequeue from client free rings\n");
            break;
        }

        memcpy(cli_buffer.phys_or_offset, thread_buffer.phys_or_offset, thread_buffer.len);

        ret = enqueue_used(&prof_cli_ring, cli_buffer);
        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to enqueue to client used rings\n");
            break;
        }

        ret = enqueue_free(&profiler_rings[ch], thread_buffer);
        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts("Failed to enqueue to thread free rings\n");
            break;
        }
    }

    /* We may not have emptied the threads rings before breaking from above loop.
        We will still resume the threads, but we will also notify the client to dump our buffers
        in the background. */
    if (ring_empty(prof_cli_ring.free_ring)) {
        /* set the thread state to waiting */
        prof_thread_waiting[ch] = true;
        microkit_notify(CLIENT_PROFILER_CH);
    } else {
        microkit_ppcall(ch, microkit_msginfo_new(PROFILER_START, 0));
    }
}

void init () {
    microkit_dbg_puts("Profiler intialising...\n");

    // Init the record buffers
    ring_init(&profiler_rings[0], (ring_buffer_t *) profiler_ring_free_t0, (ring_buffer_t *) profiler_ring_used_t0, NUM_BUFFERS);
    ring_init(&profiler_rings[1], (ring_buffer_t *) profiler_ring_free_t1, (ring_buffer_t *) profiler_ring_used_t1, NUM_BUFFERS);
    ring_init(&prof_cli_ring, (ring_buffer_t *) prof_cli_ring_free, (ring_buffer_t *) prof_cli_ring_used, PROF_CLI_NUM_BUFFERS);

    for (int i = 0; i < PROF_CLI_NUM_BUFFERS - 1; i++) {
        buff_desc_t buffer;
        buffer.phys_or_offset = prof_cli_mem + (i * sizeof(prof_sample_t));
        buffer.len = sizeof(prof_sample_t);
        int ret = enqueue_free(&prof_cli_ring, buffer);

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

// TODO: Move this over to a pp call as well.
void notified(microkit_channel ch) {
    if (ch == 10) {
        // Set the profiler state to start
        microkit_dbg_puts("Starting PMU threads\n");
        profiler_state = PROF_START;
        resume_threads();
    } else if (ch == 20) {
        // Set the profiler state to halt
        microkit_dbg_puts("Halting PMU threads\n");
        profiler_state = PROF_HALT;
        halt_threads();
        // Purge any buffers that may be leftover
        microkit_notify(CLIENT_PROFILER_CH);
    } else if (ch == 30) {
        // Only resume if profiler state is in 'START' state
        if (profiler_state == PROF_START) {
            restart_threads();
        }
    } else {
        /* This is then a notif from a thread. We want to purge all threads buffers
        to the client. Can we bypass the prof_main thread here? */
        purge_thread(ch);
    }
}
