# How to use

The profiling infrastructure currently depends on the [seL4 Microkit](https://github.com/seL4/microkit.git).

However, at this time we need a custom Microkit version as the profiling infrastructure
depends on changes made to the seL4 kernel.

## Getting Microkit

```sh
git clone https://github.com/Kswin01/sel4cp --branch microkit_prof_dev microkit
cd microkit
git clone https://github.com/Kswin01/seL4 --branch microkit-dev-profiler-irq
```

Please follow the instructions in the Microkit repository README to build the SDK.

When invoking the Microkit tool, you must specify the configuration option as `profile` in order
to make use of the profiler.

# Integrating your system

## Required Protection Domains (PDs)

Two PD's that you will need are the `profiler` PD, and the `client` PD.

The `profiler` PD is essentially the PMU driver, as well as the program responsible for creating
the samples. These samples are places into shared ringbuffers, that can be consumed by the `client`.
Additionally, you will need to setup a channel between the profiler and the profiler client:
- 30 - Used for PPC to control the profiler. Used by profiler to signal client to dump samples.

This channel number can be configured to whatever you wish, just ensure you change the `CLIENT_PROFILER_CH`
definition in `client.h`.
### Consuming profiler data

The profiler PD is responsible for formatting the sampling data in a generic format. It is the
responsibility of another user-program to consume that data and do I/O in order to analyse the data.

This project supplies a default `client` PD which is setup to consume the data using the
[seL4 Device Driver Framework](https://github.com/au-ts/sddf.git).

The supplied client can convert the profiling sample data to be transferred over the following devices:
* serial
* ethernet

It makes use of sDDF sub-systems to transfer the data in a standardised way. This means that the profiler
client is a client of the particular sub-system, just like any other PD's that may utilise the sub-system.

### Connecting profiler to client

Firstly, the profiler requires a memory region in which it stores sample data. These are:
`profiler_ring_free`, `profiler_ring_used`, `profiler_mem`. These memory regions must also be mapped
into the client.

The profiler needs to have a channel between itself and the client as mentioned before. The client can
send PPC to the profiler start/stop/restart. You will find definitions in `profiler.h` for the label
numbers for the respective control commands. Place these in the label field of the `msginfo`. The
profiler will also signal on this channel to tell the client that its buffers need emptying. The profiler
will halt the PMU until it receives a PPC to restart.

### Connecting client to subsystems

The supplied client must be connected to the virtualizers of the needed subsystem. They need to be
connected in the same manner as any other client in the system.

To connect to the different subsystems, map in the appropriate memory regions as well as create the
necessary channels. For the networking subsystem, the user must also create/modify the `ethernet_config.h`.
This is used to setup the correct memory regions for all the clients, as well as MAC addresses.

For an example of how to do this, please reference either the example system in `example/` or in the
[sDDF examples](https://github.com/au-ts/sDDF/tree/main/examples).

### Supplying your own client

However, the user can supply their own `client` PD depending on their needs. They will need to follow
the same processes as above in respect to connecting the profiler and client.

An example of how all of these components are connected can be found in `example/board/<odroidc4/maaxboard>`.

## Kernel Log Buffer

The profiler will need to be able to map in a shared memory region with the kernel, known as the kernel
log buffer. This memory region must be of 2MiB in size, and of a Large page size. Please include the
following if building using Microkit:
```xml
    <memory_region name="log_buffer" size="0x200_000" page_size="0x200_000"/>
```

## Registering a thread for profiling

The first thing the user must do is add a line of code to register a thread for profiling. This is
provided through the `seL4_ProfilerRegisterThread(int threadId)` system call. This sets a value
within the threads TCB to ensure that samples are recorded during that threads execution. The
`threadId` is a user managed identification for the thread. You will have to maintain the
appropriate mappings in `include/profiler_config.h` in order for the `perf` tools to function
correctly.

## Configuring the PMU

In the `profiler/profiler.c` `void init()` function, you can add calls to configure certain counters.
The function to configure event counters is as follows:
```c
void configure_eventcnt(int cntr, uint32_t event, uint64_t val, bool sampling)
```

For example:

```C
    /* Configures the cycle counter to sample on the period CYCLE_COUNTER_PERIOD. The 1 denotes
    that we are going to sample on the cycle counter. */

    configure_clkcnt(CYCLE_COUNTER_PERIOD, 1);

    /* Configures event counter 0 to count on L1 Data Cache accesses. Set the period to 16, and
    denote that we are going to sample on this counter. */
    configure_eventcnt(EVENT_CTR_0, L1D_CACHE, 16, 1);
```

The list of available events and counters can be found in `include/profiler.h`.
