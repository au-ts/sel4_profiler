# seL4 Profiler
This is a framework for profiling on-top of seL4. It is a statistical profiler that can be configured to 
sample based on different events and frequencies. There is a sample client that can dump packets either 
over serial or the network. 

Currently we have support for the imx8mm soc.

# Installation

- We require a special kernel configuration. The kernel version can be found at: https://github.com/Kswin01/seL4/tree/microkit-dev-profiler-irq.
- We have tested with this version of microkit: https://github.com/Ivan-Velickovic/microkit.

# Building
- Ensure that you have either downloaded the appropriate microkit SDK, or built from source. Instructions for building microkit can be found in the microkit repo.
- Use the following Make command:
```
make BUILD_DIR=<build_dir> MICROKIT_SDK=<path_to_sdk> MICROKIT_BOARD=<board> MICROKIT_CONFIG=profile
```
# Protocol Buffers

We use protocol buffers to encode our samples. Within the profiler tool, we use nanopb.
If you wish to modify the proto structure, please find the `pmu_sample.proto` file in the `protobuf/`
directory at the root of the repository.

There are two parts to protobuf in the repository.
1. Within the profiler itself, in order to encode sample data.
2. Within the netconn utility in order to parse and decode the sample data.

## Generating nanopb files

We use version 0.4.8 of nanopb, you can find the releases [here](https://jpa.kapsi.fi/nanopb/download/).

For example, to reproduce the nanopb generated files on Linux:
```sh
# In the root of the repository
wget https://jpa.kapsi.fi/nanopb/download/nanopb-0.4.8-linux-x86.tar.gz
tar xf nanopb-0.4.8-linux-x86.tar.gz
nanopb-0.4.8-linux-x86/generator-bin/protoc --nanopb_out=. protobuf/pmu_sample.proto
```

## Generating Python protobuf files

Please ensure that you have installed the Google Protocol Buffer compiler. More information can be found [here](https://github.com/protocolbuffers/protobuf).

You can install this using `pip` with the following command: `pip install protobuf`.

```sh
# In the root of the repository
protoc --python_out=protobuf/python --proto_path=protobuf/python protobuf/python/pmu_sample.proto
```

# Notes

- This system currently requires a Python client to be connected to the board over the network. The client can be found within the netconn directory. Instructions can be found in `netconn/README.md`.

- This demo system is integrated with the sDDF. Driver support will be inline with sDDF development.

- Additionally, found within the `toperf` directory is a tool to convert from the samples file outputted by the netconn tool to a `perf.data` file. This will allow us make use of exisiting infrastructure surronding perf, such as `perf report` and flamegraph tools such as Mozilla Profiler.

# Demo System
The demo system here includes two dummy programs that run in infinite loops, as well as tcp benchmark found in the `echo_server` directory. 


# How to use

## Registering a thread for profiling
The first thing the user must do is add a line of code to register a thread for profiling. This is provided through the 
`seL4_ProfilerRegisterThread(int threadId)` syscall. This sets a value within the threads TCB to ensure that 
samples are recorded during that threads execution. The `threadId` is a user managed identification for the thread. You will have to maintain the appropriate mappings in `include/profiler_config.h` in order for the perf tools to function correctly.

## Controlling the profiler
In the `include/config.h` file, there is a define for called `CLIENT_CONFIG`. Set this value to whatever control method that you wish to use. 0 is using serial, 2 is using the network controller. Serial works on both odroidc4 and imx8mm, whereas the netconn tool is only available on the imx8mm currently.

### Serial Control
- Using this profiler over serial will result in the raw json samples being dumped to terminal.
- You can entire the following values into the terminal with the multiplexer set to hand characters to the profiler:
    - 1 - Start profiling
    - 2 - Stop profiling, dump all packets

This works on the odroidc4, imx8mm and maaxboard. Ensure that the makefile is changed appropriately, and the right register addresses and IRQ's are used in the system description.

### Network Control
More details can be found in the `netconn` directory.

This only works on the imx8mm and maaxboard currently. There ethernet issues when 
using the odroidc4.

## Configuring the PMU
In the `profiler/profiler.c` `void init()` function, you can add calls to configure certain counters. The function to configure event counters is as follows:
```
void configure_eventcnt(int cntr, uint32_t event, uint64_t val, bool sampling)
```

For example:

```C
    /* Configures the cycle counter to sample on the period CYCLE_COUNTER_PERIOD. The 1 denotes that we are going to sample on the cycle counter. */

    configure_clkcnt(CYCLE_COUNTER_PERIOD, 1);

    /* Configures event counter 0 to count on L1 Data Cache accesses. Set the period to 16, and denote that we are going to sample on this counter. */
    configure_eventcnt(EVENT_CTR_0, L1D_CACHE, 16, 1);
```

The list of available events and counters can be found in `include/profiler.h`.

## Converting to Perf
The serial control will output the raw json samples. The netconn tool will output the correctly formatted json file. Please see the `toperf/` directory for more information on using the toperf tool.
