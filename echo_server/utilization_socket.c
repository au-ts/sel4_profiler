/*
 * Copyright 2020, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <string.h>
#include <microkit.h>

#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "echo.h"
#include "bench.h"
#include "util.h"

#define START_PMU 4
#define STOP_PMU 5

/* This file implements a TCP based utilization measurment process that starts
 * and stops utilization measurements based on a client's requests.
 * The protocol used to communicate is as follows:
 * - Client connects
 * - Server sends: 100 IPBENCH V1.0\n
 * - Client sends: HELLO\n
 * - Server sends: 200 OK (Ready to go)\n
 * - Client sends: LOAD cpu_target_lukem\n
 * - Server sends: 200 OK\n
 * - Client sends: SETUP args::""\n
 * - Server sends: 200 OK\n
 * - Client sends: START\n
 * - Client sends: STOP\n
 * - Server sends: 220 VALID DATA (Data to follow)\n
 *                                Content-length: %d\n
 *                                ${content}\n
 * - Server closes socket.
 *
 * It is also possible for client to send QUIT\n during operation.
 *
 * The server starts recording utilization stats when it receives START and
 * finishes recording when it receives STOP.
 *
 * Only one client can be connected.
 */

static struct tcp_pcb *utiliz_socket;
uintptr_t data_packet;
uintptr_t cyclecounters_vaddr;

#define WHOAMI "100 IPBENCH V1.0\n"
#define HELLO "HELLO\n"
#define OK_READY "200 OK (Ready to go)\n"
#define LOAD "LOAD cpu_target_lukem\n"
#define OK "200 OK\n"
#define SETUP "SETUP args::\"\"\n"
#define START "START\n"
#define STOP "STOP\n"
#define QUIT "QUIT\n"
#define RESPONSE "220 VALID DATA (Data to follow)\n"    \
    "Content-length: %d\n"                              \
    "%s\n"
#define IDLE_FORMAT ",%ld,%ld"
#define ERROR "400 ERROR\n"

#define msg_match(msg, match) (strncmp(msg, match, strlen(match))==0)

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define RES(x, y, z) "220 VALID DATA (Data to follow)\n"    \
    "Content-length: "STR(x)"\n"\
    ","STR(y)","STR(z)


struct bench *bench = (void *)(uintptr_t)0x5010000;

uint64_t start;
uint64_t idle_ccount_start;
uint64_t idle_overflow_start;


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

static err_t utilization_sent_callback(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    return ERR_OK;
}

static err_t utilization_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    if (p == NULL) {
        tcp_close(pcb);
        return ERR_OK;
    }

    char *data_packet_str = (char *)data_packet;

    pbuf_copy_partial(p, (void *)data_packet, p->tot_len, 0);
    err_t error;

    if (msg_match(data_packet_str, HELLO)) {
        error = tcp_write(pcb, OK_READY, strlen(OK_READY), TCP_WRITE_FLAG_COPY);
        if (error) {
            microkit_dbg_puts("Failed to send OK_READY message through utilization peer");
        }
    } else if (msg_match(data_packet_str, LOAD)) {
        error = tcp_write(pcb, OK, strlen(OK), TCP_WRITE_FLAG_COPY);
        if (error) {
            microkit_dbg_puts("Failed to send OK message through utilization peer");
        }
    } else if (msg_match(data_packet_str, SETUP)) {
        error = tcp_write(pcb, OK, strlen(OK), TCP_WRITE_FLAG_COPY);
        if (error) {
            microkit_dbg_puts("Failed to send OK message through utilization peer");
        }
    } else if (msg_match(data_packet_str, START)) {
        print(microkit_name);
        print(" measurement starting... \n");
        if (!strcmp(microkit_name, "client0")) {
            start = bench->ts;
            idle_ccount_start = bench->ccount;
            idle_overflow_start = bench->overflows;
            microkit_notify(START_PMU);
        }
    } else if (msg_match(data_packet_str, STOP)) {
        print(microkit_name);
        print(" measurement finished \n");;

        uint64_t total = 0, idle = 0;

        if (!strcmp(microkit_name, "client0")) {
            total = bench->ts - start;
            total += ULONG_MAX * (bench->overflows - idle_overflow_start);
            idle = bench->ccount - idle_ccount_start;
        }

        char tbuf[16];
        my_itoa(total, tbuf);

        char ibuf[16];
        my_itoa(idle, ibuf);

        char buffer[100];

        int len = strlen(tbuf) + strlen(ibuf) + 2;
        char lbuf[16];
        my_itoa(len, lbuf);

        strcat(strcpy(buffer, "220 VALID DATA (Data to follow)\nContent-length: "), lbuf);
        strcat(buffer, "\n,");
        strcat(buffer, ibuf);
        strcat(buffer, ",");
        strcat(buffer, tbuf);

        // microkit_dbg_puts(buffer);
        error = tcp_write(pcb, buffer, strlen(buffer), TCP_WRITE_FLAG_COPY);

        tcp_shutdown(pcb, 0, 1);

        if (!strcmp(microkit_name, "client0")) { 
            microkit_notify(STOP_PMU);
        }
    } else if (msg_match(data_packet_str, QUIT)) {
        /* Do nothing for now */
    } else {
        microkit_dbg_puts("Received a message that we can't handle ");
        microkit_dbg_puts(data_packet_str);
        microkit_dbg_puts("\n");
        error = tcp_write(pcb, ERROR, strlen(ERROR), TCP_WRITE_FLAG_COPY);
        if (error) {
            microkit_dbg_puts("Failed to send OK message through utilization peer");
        }
    }

    return ERR_OK;
}

static err_t utilization_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    print("Utilization connection established!\n");
    err_t error = tcp_write(newpcb, WHOAMI, strlen(WHOAMI), TCP_WRITE_FLAG_COPY);
    if (error) {
        print("Failed to send WHOAMI message through utilization peer\n");
    }
    tcp_sent(newpcb, utilization_sent_callback);
    tcp_recv(newpcb, utilization_recv_callback);

    return ERR_OK;
}

int setup_utilization_socket(void)
{
    utiliz_socket = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (utiliz_socket == NULL) {
        print("Failed to open a socket for listening!");
        return -1;
    }

    err_t error = tcp_bind(utiliz_socket, IP_ANY_TYPE, UTILIZATION_PORT);
    if (error) {
        print("Failed to bind the TCP socket");
        return -1;
    }

    utiliz_socket = tcp_listen_with_backlog_and_err(utiliz_socket, 1, &error);
    if (error != ERR_OK) {
        print("Failed to listen on the utilization socket");
        return -1;
    }
    tcp_accept(utiliz_socket, utilization_accept_callback);

    return 0;
}
