#include "serial_server.h"
#include "uart.h"
#include "shared_ringbuffer.h"
#include <string.h>
#include <stdlib.h>

/* Ring handle components -
Need to have access to the same ring buffer mechanisms as the driver, so that we can enqueue
buffers to be serviced by the driver.*/

uintptr_t rx_free_printf;
uintptr_t rx_used_printf;
uintptr_t tx_free_printf;
uintptr_t tx_used_printf;

uintptr_t shared_dma_tx_printf;
uintptr_t shared_dma_rx_printf;

struct serial_server printf_serial_server = {0};

void putchar_(char character)
{
    struct serial_server *local_server = &printf_serial_server;

    // Get a buffer from the tx ring

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    unsigned int buffer_len = 0; 
    void *cookie = 0;

    // Dequeue a buffer from the available ring from the tx buffer
    int ret = dequeue_free(&local_server->tx_ring, &buffer, &buffer_len, &cookie);

    if(ret != 0) {
        sel4cp_dbg_puts(sel4cp_name);
        sel4cp_dbg_puts(": serial server printf, unable to dequeue from tx ring, tx ring empty\n");
        return -1;
    }

    // Need to copy over the string into the buffer, if it is less than the buffer length
    int print_len = 1;

    if(print_len > BUFFER_SIZE) {
        sel4cp_dbg_puts(sel4cp_name);
        sel4cp_dbg_puts(": print string too long for buffer\n");
        return -1;
    }

    // Copy over the string to be printed to the buffer
    memcpy((char *) buffer, &character, print_len);

    // We then need to add this buffer to the transmit used ring structure

    bool is_empty = ring_empty(local_server->tx_ring.used_ring);

    ret = enqueue_used(&local_server->tx_ring, buffer, print_len, &cookie);

    if(ret != 0) {
        sel4cp_dbg_puts(sel4cp_name);
        sel4cp_dbg_puts(": serial server printf, unable to enqueue to tx used ring\n");
        return -1;
    }

    /*
    First we will check if the transmit used ring is empty. If not empty, then the driver was processing
    the used ring, however it was not finished, potentially running out of budget and being pre-empted. 
    Therefore, we can just add the buffer to the used ring, and wait for the driver to resume. However if 
    empty, then we can notify the driver to start processing the used ring.
    */

    if(is_empty) {
        // Notify the driver through the printf channel
        sel4cp_notify(SERVER_PRINT_CHANNEL);
    }

    return 0;
}

// Init function required by sel4cp, initialise serial datastructres for server here
void init_serial(void) {
    // Here we need to init ring buffers and other data structures
    sel4cp_dbg_puts("Initialising serial in serial server\n");
    struct serial_server *local_server = &printf_serial_server;
    
    // Init the shared ring buffers
    ring_init(&local_server->rx_ring, (ring_buffer_t *)rx_free_printf, (ring_buffer_t *)rx_used_printf,  0, 512, 512);
    // We will also need to populate these rings with memory from the shared dma region
    
    // Add buffers to the rx ring
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        int ret = enqueue_free(&local_server->rx_ring, shared_dma_rx_printf + (i * BUFFER_SIZE), BUFFER_SIZE, NULL);

        if (ret != 0) {
            sel4cp_dbg_puts(sel4cp_name);
            sel4cp_dbg_puts(": rx buffer population, unable to enqueue buffer\n");
        }
    }

    ring_init(&local_server->tx_ring, (ring_buffer_t *)tx_free_printf, (ring_buffer_t *)tx_used_printf, 0, 512, 512);

    // Add buffers to the tx ring
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        // Have to start at the memory region left of by the rx ring
        int ret = enqueue_free(&local_server->tx_ring, shared_dma_tx_printf + ((i + NUM_BUFFERS) * BUFFER_SIZE), BUFFER_SIZE, NULL);

        if (ret != 0) {
            sel4cp_dbg_puts(sel4cp_name);
            sel4cp_dbg_puts(": tx buffer population, unable to enqueue buffer\n");
        }
    }
}