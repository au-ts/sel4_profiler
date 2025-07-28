/* Test client for dumping profiler buffers when full */

#include <stdint.h>
#include <microkit.h>
#include <sel4/sel4.h>
#include "client.h"
#include "profiler.h"
#include <sddf/network/queue.h>
#include <sddf/network/lib_sddf_lwip.h>
#include <sddf/serial/queue.h>
#include <sddf/serial/config.h>
#include <sddf/timer/config.h>
#include <sddf/network/config.h>
#include "socket.h"
#include <string.h>
#include <serial_config.h>
#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pb_encode.h"
#include "pmu_sample.pb.h"
#include "config.h"

/* Serial communication. */
#define SERIAL_TX_CH 9
#define SERIAL_RX_CH 12

serial_queue_handle_t rx_queue_handle;
serial_queue_handle_t tx_queue_handle;

/* Profiler communication. */
// @kwinter: Switch this over to a profiler specific queue implementation
net_queue_t *profiler_active;
net_queue_t *profiler_free;
uintptr_t profiler_data_region;

net_queue_handle_t net_rx_handle;
net_queue_handle_t net_tx_handle;

#define LWIP_TICK_MS 100

struct pbuf *head;
struct pbuf *tail;

__attribute__((__section__(".profiler_config"))) profiler_config_t prof_config;
__attribute__((__section__(".serial_client_config"))) serial_client_config_t serial_config;
__attribute__((__section__(".timer_client_config"))) timer_client_config_t timer_config;
__attribute__((__section__(".net_client_config"))) net_client_config_t net_config;
__attribute__((__section__(".lib_sddf_lwip_config"))) lib_sddf_lwip_config_t lwip_config;

net_queue_handle_t profiler_queue;

// @kwinter: Fix this up - use the enum type here
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

void netif_status_callback(char *ip_addr)
{
    sddf_printf("DHCP request finished, IP address for netif %s is: %s\n", sddf_get_pd_name(), ip_addr);
}

/**
 * Stores a pbuf to be transmitted upon available transmit buffers.
 *
 * @param p pbuf to be stored.
 */
net_sddf_err_t enqueue_pbufs(struct pbuf *p)
{
    /* Indicate to the tx virt that we wish to be notified about free tx buffers */
    net_request_signal_free(&net_tx_handle);

    if (head == NULL) {
        head = p;
    } else {
        tail->next_chain = p;
    }
    tail = p;

    /* Increment reference count to ensure this pbuf is not freed by lwip */
    pbuf_ref(p);

    return SDDF_LWIP_ERR_OK;
}

char get_char() {
    char c = 0;
    serial_dequeue(&rx_queue_handle, &c);
    return c;
}

void print_dump() {
    net_buff_desc_t buffer;

    // Dequeue from the profiler used ring
    while(!net_dequeue_free(&profiler_queue, &buffer)) {
        prof_sample_t *sample = (prof_sample_t *) buffer.io_or_offset;

        sddf_printf("{\n");
        // Print out sample
        sddf_printf("\t\"ip\": \"%lx\"\n", sample->ip);
        sddf_printf("\t\"pid\": %d\n", sample->pid);
        sddf_printf("\t\"time\": \"%lu\"\n", sample->time);
        sddf_printf("\t\"cpu\": %d\n", sample->cpu);
        sddf_printf("\t\"period\": \"%ld\"\n", sample->period);
        sddf_printf("\t\"nr\": \"%ld\"\n", sample->nr);
        sddf_printf("\t\"ips\": [\n");
        for (int i = 0; i < SEL4_PROF_MAX_CALL_DEPTH; i++) {
            if (i != SEL4_PROF_MAX_CALL_DEPTH - 1) {
                sddf_printf("\t\t\"%ld\",\n", sample->ips[i]);
            } else {
                sddf_printf("\t\t\"%ld\"\n]\n", sample->ips[i]);
            }
        }
        sddf_printf("}\n");

        net_enqueue_active(&profiler_queue, buffer);
    }
}

void serial_control() {
    char ctrl_char = get_char();

    int ctrl_int = atoi(&ctrl_char);

    if (ctrl_int == 1) {
        // Start the PMU
        microkit_ppcall(prof_config.ch, microkit_msginfo_new(PROFILER_START, 0));
    } else if (ctrl_int == 2) {
        // Stop the PMU
        microkit_ppcall(prof_config.ch, microkit_msginfo_new(PROFILER_STOP, 0));
    }
}

void eth_dump() {
    net_buff_desc_t buffer;

    // Dequeue from the profiler used ring
    if (net_queue_empty_active(&profiler_queue)) {
        // If we are done dumping the buffers, we can resume the PMU
        client_state = CLIENT_IDLE;
        microkit_ppcall(prof_config.ch, microkit_msginfo_new(PROFILER_RESTART, 0));
    } else if (!net_dequeue_active(&profiler_queue, &buffer)) {
        // // Create a buffer for the sample
        uint8_t pb_buff[256];

        // // Create a pb stream from the pb_buff
        pb_ostream_t stream = pb_ostream_from_buffer(pb_buff, sizeof(pb_buff));

        prof_sample_t *sample = (prof_sample_t *) buffer.io_or_offset;

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

        net_enqueue_free(&profiler_queue, buffer);

        // We first want to send the size of the following buffer, then the buffer itself
        char size_str[8];
        my_itoa(message_len, size_str);
        send_tcp(&size_str, 2);
        send_tcp(&pb_buff, message_len);
    }

}

void init() {
    serial_queue_init(&rx_queue_handle, serial_config.rx.queue.vaddr, serial_config.rx.data.size, serial_config.rx.data.vaddr);
    serial_queue_init(&tx_queue_handle, serial_config.tx.queue.vaddr, serial_config.tx.data.size, serial_config.tx.data.vaddr);

    serial_putchar_init(serial_config.tx.id, &tx_queue_handle);

    if (CLIENT_CONFIG == CLIENT_CONTROL_NETWORK) {
            sddf_lwip_init(&lwip_config, &net_config, &timer_config, net_rx_handle, net_tx_handle, NULL,
                   netif_status_callback, enqueue_pbufs);

    }

    // Set client state to idle
    client_state = CLIENT_IDLE;

    // Init ring handle between profiler
    net_queue_init(&profiler_queue, profiler_free, profiler_active, 512);
}

void notified(microkit_channel ch) {
    // Notified to empty profiler sample buffers
    if (ch == prof_config.ch) {
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
                tcp_sent_callback((void *) eth_dump);
                eth_dump();
            }
        }
    } else if(CLIENT_CONFIG == CLIENT_CONTROL_NETWORK) {
        // Getting a network interrupt
        sddf_lwip_maybe_notify();
    } else if (ch == serial_config.rx.id) {
        // Getting a notification from serial virt rx.
        serial_control();
    }
}