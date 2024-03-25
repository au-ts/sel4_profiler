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

/* This channel is to signal profiler to start recording samples
PMU counters will be reset to the configured defaults. */
#ifndef CLIENT_START_PMU_CH
#define CLIENT_START_PMU_CH 10
#endif

/* This channel is stop recording profiling samples. */
#ifndef CLIENT_STOP_PMU_CH
#define CLIENT_STOP_PMU_CH 20
#endif

/* This channel is to resume profiling, without reseting
the PMU counters. */
#ifndef CLEINT_RESUME_PMU_CH
#define CLIENT_RESUME_PMU_CH 30
#endif

/* This channel is between the profiler and client.
The profiler will signal this when it needs the client to
consume the sample buffer. */
#ifndef CLIENT_PROFILER_CH
#define CLIENT_PROFILER_CH 5
#endif

enum client_state {
    CLIENT_IDLE,
    CLIENT_DUMP
};