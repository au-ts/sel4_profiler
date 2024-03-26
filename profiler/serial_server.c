#include "serial_server.h"
#include <microkit.h>
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <sddf/serial/queue.h>

struct serial_server {
    /* Pointers to shared_ringbuffers */
    serial_queue_handle_t rx_queue;
    serial_queue_handle_t tx_queue;
};
/* Queue handle components -
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

    // Dequeue a buffer from the available ring from the tx buffer
    int ret = serial_dequeue_free(&local_server->tx_queue, &buffer, &buffer_len);

    if(ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": serial server printf, unable to dequeue from tx ring, tx ring empty. Putchar_\n");
        return;
    }

    // Need to copy over the string into the buffer, if it is less than the buffer length
    int print_len = 1;

    if(print_len > BUFFER_SIZE) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": print string too long for buffer\n");
        return;
    }

    // Copy over the string to be printed to the buffer
    memcpy((char *) buffer, &character, print_len);

    // We then need to add this buffer to the transmit used ring structure

    bool is_empty = serial_queue_empty(local_server->tx_queue.active);

    ret = serial_enqueue_active(&local_server->tx_queue, buffer, print_len);

    if(ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": serial server printf, unable to enqueue to tx used ring\n");
        return;
    }

    /*
    First we will check if the transmit used ring is empty. If not empty, then the driver was processing
    the used ring, however it was not finished, potentially running out of budget and being pre-empted.
    Therefore, we can just add the buffer to the used ring, and wait for the driver to resume. However if
    empty, then we can notify the driver to start processing the used ring.
    */

    if(is_empty) {
        // Notify the driver through the printf channel
        microkit_notify(SERVER_PRINT_CHANNEL);
    }

    return;
}

char get_char() {
    struct serial_server *local_server = &client_serial_server;

    // Notify the driver that we want to get a character. In Patrick's design, this increments
    // the chars_for_clients value.
    microkit_notify(SERVER_GETCHAR_CHANNEL);

    /* Now that we have notified the driver, we can attempt to dequeue from the active queue.
    When the driver has processed an interrupt, it will add the inputted character to the active queue.*/

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;

    // Integer to store the length of the buffer
    unsigned int buffer_len = 0;

    while (serial_dequeue_active(&local_server->rx_queue, &buffer, &buffer_len) != 0) {
        /* The queue is currently empty, as there is no character to get.
        We will spin here until we have gotten a character. As the driver is a higher priority than us,
        it should be able to pre-empt this loop
        */
        microkit_dbg_puts(""); /* From Patrick, this is apparently needed to stop the compiler from optimising out the
        as it is currently empty. When compiled in a release version the puts statement will be compiled
        into a nop command.
        */
    }

    // We are only getting one character at a time, so we just need to cast the buffer to an int
    char got_char = *((char *) buffer);

    /* Now that we are finished with the active buffer, we can add it back to the free queue*/
    int ret = serial_enqueue_free(&local_server->rx_queue, buffer, buffer_len);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": getchar - unable to enqueue active buffer back into available queue\n");
    }

    return got_char;
}

// Init function required by microkit, initialise serial datastructres for server here
void init_serial(void) {
    // Here we need to init ring buffers and other data structures
    struct serial_server *local_server = &client_serial_server;

    // Init the shared ring buffers
    serial_queue_init(&local_server->rx_queue, (serial_queue_t *)rx_free_client, (serial_queue_t *)rx_used_client,  0, NUM_ENTRIES, NUM_ENTRIES);
    // We will also need to populate these rings with memory from the shared dma region

    // Add buffers to the rx ring
    for (int i = 0; i < NUM_ENTRIES - 1; i++) {
        int ret = serial_enqueue_free(&local_server->rx_queue, shared_dma_rx_client + (i * BUFFER_SIZE), BUFFER_SIZE);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts(": rx buffer population, unable to enqueue buffer\n");
        }
    }

    serial_queue_init(&local_server->tx_queue, (serial_queue_t *)tx_free_client, (serial_queue_t *)tx_used_client, 0, NUM_ENTRIES, NUM_ENTRIES);

    // Add buffers to the tx ring
    for (int i = 0; i < NUM_ENTRIES - 1; i++) {
        // Have to start at the memory region left of by the rx ring
        int ret = serial_enqueue_free(&local_server->tx_queue, shared_dma_tx_client + (i * BUFFER_SIZE), BUFFER_SIZE);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts(": tx buffer population, unable to enqueue buffer\n");
        }
    }
}