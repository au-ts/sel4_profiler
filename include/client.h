#pragma once

// Maybe try and expand the num of buffers in the future
#define NUM_BUFFERS 512

#define CLIENT_HALT 6
#define CLIENT_CH 5

/* CLIENT CONFIG OPTIONS
    Specify how to dump sample buffers
    0 - Print over serial
    1 - Use the xmodem protocol to send over serial
    2 - Use netconn to control and send samples over the network
*/
#define CLIENT_CONFIG 2

#define START_PMU 10
#define STOP_PMU 20
#define RESUME_PMU 30

enum client_state {
    CLIENT_IDLE,
    CLIENT_DUMP
};