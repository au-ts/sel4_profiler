#pragma once

#include <microkit.h>
#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define UDP_ECHO_PORT 1235
#define UTILIZATION_PORT 1236

#define LINK_SPEED 1000000000 // Gigabit
#define ETHER_MTU 1500
#define NUM_BUFFERS 512
#define BUF_SIZE 2048

int setup_utilization_socket(void);

void init_lwip(void);
void notified_lwip(microkit_channel ch);
int send_tcp(void *buff);
void tcp_sent_callback(tcp_sent_fn callback);
void tcp_reset_callback();