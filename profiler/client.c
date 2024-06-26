/* Test client for dumping profiler buffers when full */

#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include "client.h"
#include "profiler.h"
#include <sddf/network/shared_ringbuffer.h>
#include <sddf/serial/queue.h>
#include "serial_server.h"
#include "socket.h"
#include <string.h>
#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pb_encode.h"
#include "pmu_sample.pb.h"
#include "profiler_printf.h"

uintptr_t uart_base;

uintptr_t profiler_ring_used;
uintptr_t profiler_ring_free;
uintptr_t profiler_mem;

ring_handle_t profiler_ring;

int client_state;

static inline void my_reverse(char s[])
{
    unsigned int i, j;
    char c;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

static inline void my_itoa(uint64_t n, char s[])
{
    unsigned int i;
    uint64_t sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    my_reverse(s);
}

void print_dump() {
    buff_desc_t buffer;

    // Dequeue from the profiler used ring
    while(!dequeue_used(&profiler_ring, &buffer)) {
        prof_sample_t *sample = (prof_sample_t *) buffer.phys_or_offset;

        printf_("{\n");
        // Print out sample
        printf_("\t\"ip\": \"%lx\"\n", sample->ip);
        printf_("\t\"pid\": %d\n", sample->pid);
        printf_("\t\"time\": \"%lu\"\n", sample->time);
        printf_("\t\"cpu\": %d\n", sample->cpu);
        printf_("\t\"period\": \"%ld\"\n", sample->period);
        printf_("\t\"nr\": \"%ld\"\n", sample->nr);
        printf_("\t\"ips\": [\n");
        for (int i = 0; i < SEL4_PROF_MAX_CALL_DEPTH; i++) {
            if (i != SEL4_PROF_MAX_CALL_DEPTH - 1) {
                printf_("\t\t\"%ld\",\n", sample->ips[i]);
            } else {
                printf_("\t\t\"%ld\"\n]\n", sample->ips[i]);
            }
        } 
        printf_("}\n");

        enqueue_free(&profiler_ring, buffer);
    }
}

void serial_control() {
    char ctrl_char = get_char();

    int ctrl_int = atoi(&ctrl_char);

    if (ctrl_int == 1) {
        // Start the PMU
        microkit_ppcall(CLIENT_PROFILER_CH, microkit_msginfo_new(PROFILER_START, 0));
    } else if (ctrl_int == 2) {
        // Stop the PMU
        microkit_ppcall(CLIENT_PROFILER_CH, microkit_msginfo_new(PROFILER_STOP, 0));
    }
}

void eth_dump() {
    buff_desc_t buffer;

    // Dequeue from the profiler used ring
    if (ring_empty(profiler_ring.used_ring)) {
        // If we are done dumping the buffers, we can resume the PMU
        client_state = CLIENT_IDLE;
        microkit_ppcall(CLIENT_PROFILER_CH, microkit_msginfo_new(PROFILER_RESTART, 0));
    } else if (!dequeue_used(&profiler_ring, &buffer)) {
        // // Create a buffer for the sample
        uint8_t pb_buff[256];

        // // Create a pb stream from the pb_buff
        pb_ostream_t stream = pb_ostream_from_buffer(pb_buff, sizeof(pb_buff));

        prof_sample_t *sample = (prof_sample_t *) buffer.phys_or_offset;

        // Copy the sample to a protobuf struct
        pmu_sample pb_sample;
        pb_sample.ip = sample->ip;
        pb_sample.pid = sample->pid;
        pb_sample.time = sample->time;
        pb_sample.cpu = sample->cpu;
        pb_sample.nr = sample->nr;
        pb_sample.ips_count = SEL4_PROF_MAX_CALL_DEPTH;
        pb_sample.period = sample->period;
        for (int i = 0; i < SEL4_PROF_MAX_CALL_DEPTH; i++) {
            pb_sample.ips[i] = sample->ips[i];
        }

        // // Encode the message
        bool status = pb_encode(&stream, pmu_sample_fields, &pb_sample);
        uint32_t message_len = (uint32_t) stream.bytes_written;

        if (!status) {
            microkit_dbg_puts("Nanopb encoding failed\n");
        }

        enqueue_free(&profiler_ring, buffer);

        // We first want to send the size of the following buffer, then the buffer itself
        char size_str[8];
        my_itoa(message_len, size_str);
        send_tcp(&size_str, 2);
        send_tcp(&pb_buff, message_len);
    }

}

void init() {
    if (CLIENT_CONFIG == CLIENT_CONTROL_SERIAL) {
        init_serial();
    } else if (CLIENT_CONFIG == CLIENT_CONTROL_NETWORK) {
        microkit_dbg_puts("initialising lwip\n");
        init_lwip();
    }

    // Set client state to idle
    client_state = CLIENT_IDLE;

    // Init ring handle between profiler
    ring_init(&profiler_ring, (ring_buffer_t *) profiler_ring_free, (ring_buffer_t *) profiler_ring_used, 512);
}

void notified(microkit_channel ch) {
    // Notified to empty profiler sample buffers
    if (ch == CLIENT_PROFILER_CH) {
        // Determine how to dump buffers
        if (CLIENT_CONFIG == CLIENT_CONTROL_SERIAL) {
            // Print over serial
            print_dump();
        } else if (CLIENT_CONFIG == CLIENT_CONTROL_NETWORK) {
            // Send over TCP. Only start this if a dump is not already
            // in progress. If a dump isn't in progress, update state.
            if (client_state == CLIENT_IDLE) {
                client_state = CLIENT_DUMP;
                // Set the callback
                tcp_sent_callback(eth_dump);
                eth_dump();
            }
        }
    } else if(CLIENT_CONFIG == CLIENT_CONTROL_NETWORK) {
        // Getting a network interrupt
        notified_lwip(ch);
    } else if (ch == 11) {
        // Getting a notification from serial mux rx.
        serial_control();
    }
}