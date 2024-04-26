#pragma once

// Maybe try and expand the num of buffers in the future
#define NUM_BUFFERS 512

/* CLIENT CONFIG OPTIONS
    Specify how to dump sample buffers
    CLIENT_CONTROL_SERIAL - Print and control over serial
    CLIENT_CONTROL_NETWORK - Use netconn to control and send samples over the network
*/

enum client_control_type {
    CLIENT_CONTROL_SERIAL,
    CLIENT_CONTROL_NETWORK
};

#define CLIENT_CONFIG CLIENT_CONTROL_NETWORK

/* This channel is between the profiler and client.
The profiler will signal this when it needs the client to
consume the sample buffer. The client will use this to send PPC
to control the profiler. */
#ifndef CLIENT_PROFILER_CH
#define CLIENT_PROFILER_CH 30
#endif

enum client_state {
    CLIENT_IDLE,
    CLIENT_DUMP
};