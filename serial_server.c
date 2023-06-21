#include "serial_server.h"
#include "serial.h"
#include "shared_ringbuffer.h"
#include <string.h>
#include <stdlib.h>

/* Ring handle components -
Need to have access to the same ring buffer mechanisms as the driver, so that we can enqueue
buffers to be serviced by the driver.*/

uintptr_t rx_avail;
uintptr_t rx_used;
uintptr_t tx_avail;
uintptr_t tx_used;

uintptr_t shared_dma;

struct serial_server global_serial_server = {0};

/*
Return -1 on failure.
*/
int serial_server_printf(char *string) {
    //sel4cp_dbg_puts("Beginning serial server printf\n");
    struct serial_server *local_server = &global_serial_server;
    // Get a buffer from the tx ring

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    unsigned int buffer_len = 0; 
    void *cookie = 0;

    //sel4cp_dbg_puts("Attempting to dequeue from the tx available ring\n");

    // Dequeue a buffer from the available ring from the tx buffer
    int ret = dequeue_avail(&local_server->tx_ring, &buffer, &buffer_len, &cookie);

    if(ret != 0) {
        //sel4cp_dbg_puts(sel4cp_name);
        //sel4cp_dbg_puts(": serial server printf, unable to dequeue from tx ring, tx ring empty\n");
        return -1;
    }

    // Need to copy over the string into the buffer, if it is less than the buffer length
    int print_len = strlen(string) + 1;

    if(print_len > BUFFER_SIZE) {
        //sel4cp_dbg_puts(sel4cp_name);
        //sel4cp_dbg_puts(": print string too long for buffer\n");
        return -1;
    }

    //sel4cp_dbg_puts("Attempting memcpy to buffer\n");
    // Copy over the string to be printed to the buffer
    memcpy((char *) buffer, string, print_len);

    // We then need to add this buffer to the transmit used ring structure

    bool is_empty = ring_empty(local_server->tx_ring.used_ring);

    ret = enqueue_used(&local_server->tx_ring, buffer, print_len, &cookie);

    if(ret != 0) {
        //sel4cp_dbg_puts(sel4cp_name);
        //sel4cp_dbg_puts(": serial server printf, unable to enqueue to tx used ring\n");
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
        //sel4cp_dbg_puts("Notifying the driver that we have something to print\n");
        sel4cp_notify(SERVER_PRINT_CHANNEL);
    }

    return 0;
}

void putchar_(char character)
{
    // Add code to print a single character to some device
        //sel4cp_dbg_puts("Beginning serial server printf\n");
    struct serial_server *local_server = &global_serial_server;
    // Get a buffer from the tx ring

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    unsigned int buffer_len = 0; 
    void *cookie = 0;

    //sel4cp_dbg_puts("Attempting to dequeue from the tx available ring\n");

    // Dequeue a buffer from the available ring from the tx buffer
    int ret = dequeue_avail(&local_server->tx_ring, &buffer, &buffer_len, &cookie);

    if(ret != 0) {
        //sel4cp_dbg_puts(sel4cp_name);
        //sel4cp_dbg_puts(": serial server printf, unable to dequeue from tx ring, tx ring empty\n");
        return -1;
    }

    // Need to copy over the string into the buffer, if it is less than the buffer length
    int print_len = 1;

    if(print_len > BUFFER_SIZE) {
        //sel4cp_dbg_puts(sel4cp_name);
        //sel4cp_dbg_puts(": print string too long for buffer\n");
        return -1;
    }

    //sel4cp_dbg_puts("Attempting memcpy to buffer\n");
    // Copy over the string to be printed to the buffer
    memcpy((char *) buffer, &character, print_len);

    // We then need to add this buffer to the transmit used ring structure

    bool is_empty = ring_empty(local_server->tx_ring.used_ring);

    ret = enqueue_used(&local_server->tx_ring, buffer, print_len, &cookie);

    if(ret != 0) {
        //sel4cp_dbg_puts(sel4cp_name);
        //sel4cp_dbg_puts(": serial server printf, unable to enqueue to tx used ring\n");
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
        //sel4cp_dbg_puts("Notifying the driver that we have something to print\n");
        sel4cp_notify(SERVER_PRINT_CHANNEL);
    }

    return 0;
}

// Return char on success, -1 on failure
int getchar() {
    //sel4cp_dbg_puts("Beginning serial server getchar\n");

    // Notify the driver that we want to get a character. In Patrick's design, this increments 
    // the chars_for_clients value.
    sel4cp_notify(SERVER_GETCHAR_CHANNEL);

    struct serial_server *local_server = &global_serial_server;

    /* Now that we have notified the driver, we can attempt to dequeue from the used ring.
    When the driver has processed an interrupt, it will add the inputted character to the used ring.*/
    
    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    unsigned int buffer_len = 0; 

    void *cookie = 0;

    //sel4cp_dbg_puts("Busy waiting until we are able to dequeue something from the rx ring buffer\n");
    while (dequeue_used(&local_server->rx_ring, &buffer, &buffer_len, &cookie) != 0) {
        /* The ring is currently empty, as there is no character to get. 
        We will spin here until we have gotten a character. As the driver is a higher priority than us, 
        it should be able to pre-empt this loop
        */
        sel4cp_dbg_puts(""); /* From Patrick, this is apparently needed to stop the compiler from optimising out the 
        as it is currently empty. When compiled in a release version the puts statement will be compiled
        into a nop command.
        */
    }

    //sel4cp_dbg_puts("Finished looping, dequeue used buffer successfully\n");

    // We are only getting one character at a time, so we just need to cast the buffer to an int

    char got_char = *((char *) buffer);

    /* Now that we are finished with the used buffer, we can add it back to the available ring*/

    int ret = enqueue_avail(&local_server->rx_ring, buffer, buffer_len, NULL);

    if (ret != 0) {
        //sel4cp_dbg_puts(sel4cp_name);
        //sel4cp_dbg_puts(": getchar - unable to enqueue used buffer back into available ring\n");
    }

    //sel4cp_dbg_puts("Finished the server getchar function\n");
    return (int) got_char;
}

/* Return 0 on success, -1 on failure. 
Basic scanf implementation using the getchar function above. Gets characters until
CTRL+C or CTRL+D or new line.
NOT MEMORY SAFE
*/ 
int serial_server_scanf(char* buffer) {
    int i = 0;
    int getchar_ret = getchar();

    if (getchar_ret == -1) {
        //sel4cp_dbg_puts("Error getting char\n");
        return -1;
    }


    while(getchar_ret != '\03' && getchar_ret != '\04' && getchar_ret != '\r') {
        ((char *) buffer)[i] = (char) getchar_ret;

        getchar_ret = getchar();

        if (getchar_ret == -1) {
            //sel4cp_dbg_puts("Error getting char\n");
            return -1;
        }

        i++;
    }

    return 0;
}

// Init function required by sel4cp, initialise serial datastructres for server here
void init_serial(void) {
    // sel4cp_dbg_puts(sel4cp_name);
    // sel4cp_dbg_puts(": elf PD init function running\n");

    // Here we need to init ring buffers and other data structures

    struct serial_server *local_server = &global_serial_server;
    
    // Init the shared ring buffers
    ring_init(&local_server->rx_ring, (ring_buffer_t *)rx_avail, (ring_buffer_t *)rx_used, NULL, 0);
    // We will also need to populate these rings with memory from the shared dma region
    
    //sel4cp_dbg_puts("populating rx ring with buffers\n");

    // Add buffers to the rx ring
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        int ret = enqueue_avail(&local_server->rx_ring, shared_dma + (i * BUFFER_SIZE), BUFFER_SIZE, NULL);

        if (ret != 0) {
            //sel4cp_dbg_puts(sel4cp_name);
            //sel4cp_dbg_puts(": rx buffer population, unable to enqueue buffer\n");
        }
    }

    //sel4cp_dbg_puts("populating tx ring with buffers\n");

    ring_init(&local_server->tx_ring, (ring_buffer_t *)tx_avail, (ring_buffer_t *)tx_used, NULL, 0);

    // Add buffers to the tx ring
    for (int i = 0; i < NUM_BUFFERS - 1; i++) {
        // Have to start at the memory region left of by the rx ring
        int ret = enqueue_avail(&local_server->tx_ring, shared_dma + ((i + NUM_BUFFERS) * BUFFER_SIZE), BUFFER_SIZE, NULL);

        if (ret != 0) {
            //sel4cp_dbg_puts(sel4cp_name);
            //sel4cp_dbg_puts(": tx buffer population, unable to enqueue buffer\n");
        }
    }

    
    
}