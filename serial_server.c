#include "serial_server.h"
#include "uart.h"
#include "shared_ringbuffer.h"
#include <string.h>
#include <stdlib.h>

/* Ring handle components -
Need to have access to the same ring buffer mechanisms as the driver, so that we can enqueue
buffers to be serviced by the driver.*/

uintptr_t rx_free_client;
uintptr_t rx_used_client;
uintptr_t tx_free_client;
uintptr_t tx_used_client;

uintptr_t shared_dma_tx_client;
uintptr_t shared_dma_rx_client;

struct serial_server client_serial_server = {0};

void putchar_(char character)
{
    struct serial_server *local_server = &client_serial_server;

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

/* XMODEM FUNCTIONS */
void _outbyte(int c) {
    struct serial_server *local_server = &client_serial_server;

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
    memcpy((char *) buffer, c, print_len);

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

void _outbuff(char *buff, unsigned int len) {
    /* Similair to _outbyte, but we will copy over the entire buffer at a time, rather than bytes.
    Check to see how many ring buffers we will need, and if the supplied buffer will fit before 
    dequeueing from the rings. */
    struct serial_server *local_server = &client_serial_server;

    int buffs_required = 0;
    
    // TO-DO: Improve this checking
    if (len < BUFFER_SIZE) {
        buffs_required = 1;
    } else {
        buffs_required = (len/BUFFER_SIZE) + 1;
    }

    if ((NUM_BUFFERS - ring_size(&local_server->tx_ring)) < buffs_required) {
        sel4cp_dbg_puts("Not enough buffers availble in _outbuff\n");
        return;
    }
    
    // To track if we need to notify our driver after we have finished copying buffers across
    bool is_empty = false;

    // Dequeue all the buffers required
    for (int i = 0; i < buffs_required; i++) {
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
        int print_len = len;

        if(print_len > BUFFER_SIZE) {
            sel4cp_dbg_puts(sel4cp_name);
            sel4cp_dbg_puts(": print string too long for buffer\n");
            return -1;
        }

        // Copy over the string to be printed to the buffer
        memcpy((char *) buffer, &buff, print_len);

        // We then need to add this buffer to the transmit used ring structure

        is_empty = ring_empty(local_server->tx_ring.used_ring);

        ret = enqueue_used(&local_server->tx_ring, buffer, print_len, &cookie);

        if(ret != 0) {
            sel4cp_dbg_puts(sel4cp_name);
            sel4cp_dbg_puts(": serial server printf, unable to enqueue to tx used ring\n");
            return -1;
        }
    }

    if(is_empty) {
        // Notify the driver through the printf channel
        sel4cp_notify(SERVER_PRINT_CHANNEL);
    }

    return 0;
}


int _inbyte(unsigned int timeout) {
    // Notify the driver that we want to get a character. In Patrick's design, this increments 
    // the chars_for_clients value.
    sel4cp_notify(SERVER_GETCHAR_CHANNEL);

    struct serial_server *local_server = &client_serial_server;

    /* Now that we have notified the driver, we can attempt to dequeue from the used ring.
    When the driver has processed an interrupt, it will add the inputted character to the used ring.*/
    
    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    unsigned int buffer_len = 0; 

    void *cookie = 0;

    while (dequeue_used(&local_server->rx_ring, &buffer, &buffer_len, &cookie) != 0) {
        /* The ring is currently empty, as there is no character to get. 
        We will spin here until we have gotten a character. As the driver is a higher priority than us, 
        it should be able to pre-empt this loop
        */
        asm("nop");
    }

    // We are only getting one character at a time, so we just need to cast the buffer to an int

    char got_char = *((char *) buffer);

    /* Now that we are finished with the used buffer, we can add it back to the free ring*/

    int ret = enqueue_free(&local_server->rx_ring, buffer, buffer_len, NULL);

    if (ret != 0) {
        sel4cp_dbg_puts(sel4cp_name);
        sel4cp_dbg_puts(": getchar - unable to enqueue used buffer back into free ring\n");
    }

    return (int) got_char;
}

// Init function required by sel4cp, initialise serial datastructres for server here
void init_serial(void) {
    // Here we need to init ring buffers and other data structures
    sel4cp_dbg_puts("Initialising serial in serial server\n");
    struct serial_server *local_server = &client_serial_server;
    
    // Init the shared ring buffers
    ring_init(&local_server->rx_ring, (ring_buffer_t *)rx_free_client, (ring_buffer_t *)rx_used_client,  0, 512, 512);
    // We will also need to populate these rings with memory from the shared dma region
    
    // Add buffers to the rx ring
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        int ret = enqueue_free(&local_server->rx_ring, shared_dma_rx_client + (i * BUFFER_SIZE), BUFFER_SIZE, NULL);

        if (ret != 0) {
            sel4cp_dbg_puts(sel4cp_name);
            sel4cp_dbg_puts(": rx buffer population, unable to enqueue buffer\n");
        }
    }

    ring_init(&local_server->tx_ring, (ring_buffer_t *)tx_free_client, (ring_buffer_t *)tx_used_client, 0, 512, 512);

    // Add buffers to the tx ring
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        // Have to start at the memory region left of by the rx ring
        int ret = enqueue_free(&local_server->tx_ring, shared_dma_tx_client + ((i + NUM_BUFFERS) * BUFFER_SIZE), BUFFER_SIZE, NULL);

        if (ret != 0) {
            sel4cp_dbg_puts(sel4cp_name);
            sel4cp_dbg_puts(": tx buffer population, unable to enqueue buffer\n");
        }
    }
}