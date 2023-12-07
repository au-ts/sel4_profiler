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
#include "socket.h"
#include <string.h>
#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "nanopb/pb_encode.h"
#include "nanopb/pmu_sample.pb.h"

uintptr_t uart_base;
uintptr_t profiler_control;

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
    uintptr_t buffer = 0;
    unsigned int size = 0;
    void *cookie = 0;

    // Dequeue from the profiler used ring
    while(!dequeue_used(&profiler_ring, &buffer, &size, &cookie)) {
        pmu_sample_t *sample = (pmu_sample_t *) buffer;

        printf_("{\n");
        // Print out sample
        printf_("\t\"ip\":\"%lx\"\n", sample->ip);
        printf_("\t\"pd\":\"%d\"\n", sample->pid);
        printf_("\t\"timestamp\":\"%lu\"\n", sample->time);
        printf_("\t\"cpu\":\"%d\"\n", sample->cpu);
        printf_("\t\"period\":\"%ld\"\n", sample->period);
        printf_("}\n");

        enqueue_free(&profiler_ring, buffer, size, cookie);
    }
}

// TODO: Fix xmodem implementation. 
void xmodem_dump() {
    uintptr_t buffer = 0;
    unsigned int size = 0;
    void *cookie = 0;

    // Dequeue from the profiler used ring
    while(!dequeue_used(&profiler_ring, &buffer, &size, &cookie)) {
        dequeue_used(&profiler_ring, &buffer, &size, &cookie);
        int ret = xmodemTransmit((unsigned char *) buffer, 128);
        sel4cp_dbg_puts("This is the ret of xmodem: ");
        puthex64(ret);
        sel4cp_dbg_puts("\n");
        enqueue_free(&profiler_ring, buffer, size, cookie);
    }   
}

static err_t eth_dump_callback(void *arg, struct tcp_pcb *pcb, uint16_t len)
{
    // sel4cp_dbg_puts("Eth dump callback\n");
    uintptr_t buffer = 0;
    unsigned int size = 0;
    void *cookie = 0;

    // Check if we are done sending buffers. 
    // If we are done, set client state to idle and signal profiler to restart profiling
    if (ring_empty(profiler_ring.used_ring)) {
        tcp_reset_callback();
        client_state = CLIENT_IDLE;
        sel4cp_notify(30);
    } else if (!dequeue_used(&profiler_ring, &buffer, &size, &cookie)) {
        // sel4cp_dbg_puts("Sending initial buffer\n");

        // // Create a buffer for the sample
        uint8_t pb_buff[256];

        // // Create a pb stream from the pb_buff
        pb_ostream_t stream = pb_ostream_from_buffer(pb_buff, sizeof(pb_buff));

        pmu_sample_t *sample = (pmu_sample_t *) buffer;

        // Copy the sample to a protobuf struct
        pmu_sample pb_sample;
        pb_sample.ip = sample->ip;
        pb_sample.pid = sample->pid;
        pb_sample.time = sample->time;
        pb_sample.cpu = sample->cpu;
        pb_sample.ips_count = MAX_INSN;
        pb_sample.period = sample->period;
        for (int i = 0; i < MAX_INSN; i++) {
            pb_sample.ips[i] = sample->ips[i];
        }

        // // Encode the message
        bool status = pb_encode(&stream, pmu_sample_fields, &pb_sample);
        uint64_t message_len = (uint64_t) stream.bytes_written;

        if (!status) {
            sel4cp_dbg_puts("Nanopb encoding failed\n");
        }

        enqueue_free(&profiler_ring, buffer, size, cookie);
        
        // We first want to send the size of the following buffer, then the buffer itself
        char size_str[16];
        my_itoa(message_len, size_str);
        sel4cp_dbg_puts("This is the value of size_conv: ");
        sel4cp_dbg_puts(size_str);
        sel4cp_dbg_puts("\n");
        send_tcp(&size_str);
        send_tcp(&pb_buff);
    }


    return ERR_OK;
}

void eth_dump_start() {
    // Set the callback
    tcp_sent_callback(eth_dump_callback);
    uintptr_t buffer = 0;
    unsigned int size = 0;
    void *cookie = 0;

    // Dequeue from the profiler used ring
    if (ring_empty(profiler_ring.used_ring)) {
        // If we are done dumping the buffers, we can resume the PMU
        client_state = CLIENT_IDLE;
        sel4cp_notify(RESUME_PMU);
    } else if (!dequeue_used(&profiler_ring, &buffer, &size, &cookie)) {
        // sel4cp_dbg_puts("Sending initial buffer\n");

        // // Create a buffer for the sample
        uint8_t pb_buff[256];

        // // Create a pb stream from the pb_buff
        pb_ostream_t stream = pb_ostream_from_buffer(pb_buff, sizeof(pb_buff));

        pmu_sample_t *sample = (pmu_sample_t *) buffer;

        // Copy the sample to a protobuf struct
        pmu_sample pb_sample;
        pb_sample.ip = sample->ip;
        pb_sample.pid = sample->pid;
        pb_sample.time = sample->time;
        pb_sample.cpu = sample->cpu;
        pb_sample.ips_count = MAX_INSN;
        pb_sample.period = sample->period;
        for (int i = 0; i < MAX_INSN; i++) {
            pb_sample.ips[i] = sample->ips[i];
        }

        // // Encode the message
        bool status = pb_encode(&stream, pmu_sample_fields, &pb_sample);
        uint64_t message_len = (uint64_t) stream.bytes_written;

        if (!status) {
            sel4cp_dbg_puts("Nanopb encoding failed\n");
        }

        enqueue_free(&profiler_ring, buffer, size, cookie);

        // We first want to send the size of the following buffer, then the buffer itself
        char size_str[16];
        my_itoa(message_len, size_str);
        sel4cp_dbg_puts("This is the value of size_conv: ");
        sel4cp_dbg_puts(size_str);
        sel4cp_dbg_puts("\n");
        send_tcp(&size_str);
        send_tcp(&pb_buff);
    }
    
}

void init() {
    // Init serial here 
    // Currently handled by profiler, but eventually remove everything from there
    if (CLIENT_CONFIG == 0 || CLIENT_CONFIG == 1) {
        init_serial();
    } else if (CLIENT_CONFIG == 2) {
        init_lwip();
    }

    // Set client state to idle
    client_state = CLIENT_IDLE;

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
        } else if (CLIENT_CONFIG == 2) {
            // Send over TCP. Only start this if a dump is not already
            // in progress. If a dump isn't in progress, update state.
            if (client_state == CLIENT_IDLE) {
                client_state = CLIENT_DUMP;
                eth_dump_start();
            }
        }
    } else if(CLIENT_CONFIG == 2) {
        // Getting a network interrupt
        notified_lwip(ch);
    } 
}