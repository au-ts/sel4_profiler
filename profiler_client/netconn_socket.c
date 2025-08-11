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

#include <microkit.h>

#include <sddf/util/string.h>
#include <sddf/util/printf.h>

#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

#include "socket.h"
#include "client.h"
#include "profiler.h"
#include "config.h"

#define MAX_PACKET_SIZE 0x1000

extern profiler_config_t prof_config;

/* This file implements a TCP based profiler control process that starts
 * and stops the profiler based on a client's requests.
 * The protocol used to communicate is as follows:
 * - Client connects
 * - Server sends: 100 SEL4 PROFILING CLIENT\n
 * - Client sends: HELLO\n
 * - Server sends: 200 OK (Ready to go)\n
 * - Client sends: START\n
 * - Client sends: STOP\n
 * - Client sends: START\n
 * - Client sends: EXIT\n
 * - Server closes socket.
 *
 *
 * The server starts recording profiling samples when it receives START and
 * finishes recording when it receives STOP or EXIT.
 *
 * Only one client can be connected.
 */

static struct tcp_pcb *utiliz_socket;
char data_packet_str[MAX_PACKET_SIZE];

#define WHOAMI "100 SEL4 PROFILING CLIENT\n"
#define HELLO "HELLO\n"
#define OK_READY "200 OK (Ready to go)\n"
#define OK "200 OK\n"
#define START "START\n"
#define STOP "STOP\n"
#define QUIT "QUIT\n"
#define ERROR "400 ERROR\n"
#define MAPPINGS "MAPPINGS\n"
#define REFRESH "REFRESH\n"

#define msg_match(msg, match) (sddf_strncmp(msg, match, sddf_strlen(match))==0)


static err_t netconn_sent_callback(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    return ERR_OK;
}

static err_t netconn_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    if (p == NULL) {
        tcp_close(pcb);
        return ERR_OK;
    }


    pbuf_copy_partial(p, (void *)data_packet_str, p->tot_len, 0);
    err_t error;

    if (msg_match(data_packet_str, HELLO)) {
        error = tcp_write(pcb, OK_READY, sddf_strlen(OK_READY), TCP_WRITE_FLAG_COPY);
        if (error) {
            sddf_dprintf("Failed to send OK_READY message through netconn peer");
        }
    } else if (msg_match(data_packet_str, START)) {
        microkit_ppcall(prof_config.ch, microkit_msginfo_new(PROFILER_START, 0));
    } else if (msg_match(data_packet_str, STOP)) {
        microkit_ppcall(prof_config.ch, microkit_msginfo_new(PROFILER_STOP, 0));
    } else if (msg_match(data_packet_str, REFRESH)) {
        // This is just to refresh the socket from the linux client side
        return ERR_OK;
    } else {
        sddf_dprintf("Received a message that we can't handle ");
        sddf_dprintf(data_packet_str);
        sddf_dprintf("\n");
        error = tcp_write(pcb, ERROR, sddf_strlen(ERROR), TCP_WRITE_FLAG_COPY);
        if (error) {
            sddf_dprintf("Failed to send OK message through netconn peer");
        }
    }

    return ERR_OK;
}

static err_t netconn_accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    sddf_dprintf("Netconn connection established!\n");
    err_t error = tcp_write(newpcb, WHOAMI, sddf_strlen(WHOAMI), TCP_WRITE_FLAG_COPY);
    if (error) {
        sddf_dprintf("Failed to send WHOAMI message through netconn peer\n");
    }
    tcp_sent(newpcb, netconn_sent_callback);
    tcp_recv(newpcb, netconn_recv_callback);
    utiliz_socket = newpcb;
    return ERR_OK;
}

int send_tcp(void *buff, uint32_t len) {

    err_t error = tcp_write(utiliz_socket, buff, len, TCP_WRITE_FLAG_COPY);
    if (error) {
        sddf_dprintf("Failed to send message through netconn peer: %d\n", error);
        if (error == -1) {
            sddf_dprintf("MEM ERROR\n");
        }
        return 1;
    }

    error = tcp_output(utiliz_socket);
    if (error) {
        sddf_dprintf("Failed to output via tcp through netconn peer: %d\n", error);
    }

    return 0;
}

void tcp_sent_callback(tcp_sent_fn callback) {
    tcp_sent(utiliz_socket, callback);
}

void tcp_reset_callback() {
    tcp_sent(utiliz_socket, netconn_sent_callback);
}

int setup_netconn_socket(void)
{
    utiliz_socket = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (utiliz_socket == NULL) {
        sddf_dprintf("Failed to open a socket for listening!");
        return -1;
    }

    err_t error = tcp_bind(utiliz_socket, IP_ANY_TYPE, NETCONN_PORT);
    if (error) {
        sddf_dprintf("Failed to bind the TCP socket");
        return -1;
    } else {
        sddf_dprintf("Netconn socket bound to port 1236\n");
    }

    utiliz_socket = tcp_listen_with_backlog_and_err(utiliz_socket, 1, &error);
    if (error != ERR_OK) {
        sddf_dprintf("Failed to listen on the netconn socket");
        return -1;
    }
    tcp_accept(utiliz_socket, netconn_accept_callback);
    return 0;
}