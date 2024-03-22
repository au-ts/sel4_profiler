#pragma once

#include <microkit.h>
#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define NETCONN_PORT 1236

#define LINK_SPEED 1000000000 // Gigabit
#define ETHER_MTU 1500

int setup_netconn_socket(void);

void init_lwip(void);
void notified_lwip(microkit_channel ch);
int send_tcp(void *buff, uint32_t len);
void tcp_sent_callback(tcp_sent_fn callback);
void tcp_reset_callback();