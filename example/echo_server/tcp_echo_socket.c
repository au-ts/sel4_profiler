#include <string.h>
#include <microkit.h>

#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "timer.h"

#include "echo.h"

#define TCP_ECHO_PORT 1237

uintptr_t tcp_recv_buffer;

static struct tcp_pcb *tcp_socket;

static err_t lwip_tcp_sent_callback(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    return ERR_OK;
}

static err_t lwip_tcp_recv_callback(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    if (p == NULL) {
        sddf_printf("Closing conn\n");
        err = tcp_close(pcb);
        if (err) {
            sddf_printf("Error closing\n");
        }
        return ERR_OK;
    }
    
    pbuf_copy_partial(p, (void *)tcp_recv_buffer, p->tot_len, 0);

    err = tcp_write(pcb, (void *)tcp_recv_buffer, p->tot_len, TCP_WRITE_FLAG_COPY);
    if (err) {
        err = tcp_output(pcb);
        if (err) {
            sddf_printf("Message can't send\n");
        }
    }

    tcp_recved(pcb, p->tot_len);
    
    pbuf_free(p);

    return ERR_OK;
    
}

void lwip_tcp_err_callback(void *arg, err_t err) {

    switch (err) {
        case ERR_RST:
            sddf_printf("Connection reset by peer\n");
            struct pcb *tcp_pcb = (struct pcb*)arg;
            tcp_close(tcp_pcb);
            break;
        case ERR_ABRT:
            sddf_printf("Connection aborted\n");
            break;

        default:
            sddf_printf("ERROR HAS OCCURED\n");
            sddf_printf('\n');

    }
    
}

static err_t tcp_accept_callback(void *arg, struct tcp_pcb *pcb, err_t err) {
    sddf_printf("TCP CONNECTED\n");

    tcp_nagle_disable(pcb);

    tcp_arg(pcb, (void *)pcb);
    tcp_recv(pcb, lwip_tcp_recv_callback);
    tcp_err(pcb, lwip_tcp_err_callback);

    return ERR_OK;
}



int setup_tcp_socket(void)
{
    tcp_socket = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (tcp_socket == NULL) {
        microkit_dbg_puts("Failed to open a TCP socket");
        return -1;
    }

    int error = tcp_bind(tcp_socket, IP_ANY_TYPE, TCP_ECHO_PORT);
    if (error == ERR_OK) {
        tcp_socket = tcp_listen(tcp_socket);

        tcp_accept(tcp_socket, tcp_accept_callback);
    } else {
        microkit_dbg_puts("Failed to bind the TCP socket");
        return -1;
    }

    return 0;
}