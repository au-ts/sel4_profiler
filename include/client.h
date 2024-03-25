#pragma once

// Maybe try and expand the num of buffers in the future
#define NUM_BUFFERS 512

#define CLIENT_HALT 6
#define CLIENT_CH 5

/* CLIENT CONFIG OPTIONS
    Specify how to dump sample buffers
    0 - Print over serial
    1 - Use netconn to control and send samples over the network
*/

enum client_control_type {
    CLIENT_CONTROL_SERIAL,
    CLIENT_CONTROL_NETWORK
};

#define CLIENT_CONFIG CLIENT_CONTROL_NETWORK

#ifndef CLIENT_START_PMU_CH
#define CLIENT_START_PMU_CH 10
#endif

#ifndef CLIENT_STOP_PMU_CH
#define CLIENT_STOP_PMU_CH 20
#endif

#ifndef CLEINT_RESUME_PMU_CH
#define CLIENT_RESUME_PMU_CH 30
#endif

#define START_PMU 10
#define STOP_PMU 20
#define RESUME_PMU 30

enum client_state {
    CLIENT_IDLE,
    CLIENT_DUMP
};