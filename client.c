/* Test client for dumping profiler buffers when full */

#include <stdint.h>
#include <sel4cp.h>
#include <sel4/sel4.h>
#include "client.h"
#include "perf.h"
#include "printf.h"
#include "profiler.h"
#include "xmodem.h"
#include "shared_ringbuffer.h"
#include "serial_server.h"

uintptr_t uart_base;
uintptr_t profiler_control;

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_free;
uintptr_t profiler_mem;

ring_handle_t profiler_ring;

void print_dump() {
    uintptr_t buffer = 0;
    unsigned int size = 0;
    void *cookie = 0;

    // Dequeue from the profiler used ring
    while(!dequeue_used(&profiler_ring, &buffer, &size, &cookie)) {
        perf_sample_t *sample = (perf_sample_t *) buffer;

        printf_("{\n");
        // Print out sample
        printf_("\t\"ip\":\"%lx\"\n", sample->ip);
        printf_("\t\"pid\":\"%d\"\n", sample->pid);
        printf_("\t\"time\":\"%lu\"\n", sample->time);
        printf_("\t\"addr\":\"%lu\"\n", sample->addr);
        printf_("\t\"id\":\"%lu\"\n", sample->id);
        printf_("\t\"stream_id\":\"%lu\"\n", sample->stream_id);
        printf_("\t\"cpu\":\"%d\"\n", sample->cpu);
        printf_("\t\"period\":\"%ld\"\n", sample->period);
        printf_("}\n");

        enqueue_free(&profiler_ring, buffer, size, cookie);
    }
}

void xmodem_dump() {
    uintptr_t buffer = 0;
    unsigned int size = 0;
    void *cookie = 0;

    // Dequeue from the profiler used ring
    // while(!dequeue_used(&profiler_ring, &buffer, &size, &cookie)) {
        dequeue_used(&profiler_ring, &buffer, &size, &cookie);
        perf_sample_t *sample = (perf_sample_t *) buffer;
        int ret = xmodemTransmit(&buffer, 128);
        sel4cp_dbg_puts("This is the ret of xmodem: ");
        puthex64(ret);
        sel4cp_dbg_puts("\n");
        enqueue_free(&profiler_ring, buffer, size, cookie);
    // }   
}

void init() {
    // Init serial here 
    // Currently handled by profiler, but eventually remove everything from there
    init_serial();

    // Init ring handle between profiler
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 0, 512, 512);
}

void notified(sel4cp_channel ch) {
    // Notified to empty profiler sample buffers
    if (ch == CLIENT_CH) {
        // Determine how to dump buffers
        if (CLIENT_CONFIG == 0) {
            // Print over serial
            print_dump();
        } else if (CLIENT_CONFIG == 1) {
            // Send over serial using xmodem protocol
            xmodem_dump();
        }
    }
}