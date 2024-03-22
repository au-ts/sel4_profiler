# seL4 Profiler
This is a framework for profiling on-top of seL4. It is a statistical profiler that can be configured to 
sample based on different events and frequencies. There is a sample client that can dump packets either 
over serial or the network. 

Currently, we support armv8 platforms. Specifically, we have tested on the `maaxboard` and `odroidc4`. The profiler component should be SoC agnostic, however, the client is reliant on the current seL4 `sDDF` development. 

# Project Status
Currently completed items:
- PMU interface for arm8.
- Kernel changes to record sample data (instruction pointer, callstack).
- Statistical profiler.
- Client to send samples over serial/ethernet.
- Using protocol buffers to send data over ethernet.
- Conversion tool to generate `perf.data` files. 

TO-DO:
- Multicore support.

# Installation

- We require a special kernel configuration. The kernel version can be found at: https://github.com/Kswin01/seL4/tree/microkit-dev-profiler-irq.
- We have tested with this version of Microkit: https://github.com/Kswin01/sel4cp/tree/microkit_prof_dev.

# Building example
- Ensure that you have either downloaded the appropriate Microkit SDK, or built from source. Instructions for building Microkit can be found in the Microkit repo.
- Please check the `example` directory for build instructions.

# Protocol Buffers

We use protocol buffers to encode our samples. Within the profiler tool, we use `nanopb`.
If you wish to modify the proto structure, please find the `pmu_sample.proto` file in the `protobuf/`
directory at the root of the repository.

There are two parts to `protobuf` in the repository.
1. Within the profiler itself, in order to encode sample data.
2. Within the `netconn` utility in order to parse and decode the sample data.

## Generating nanopb files

We use version 0.4.8 of `nanopb`, you can find the releases [here](https://jpa.kapsi.fi/nanopb/download/).

For example, to reproduce the `nanopb` generated files on Linux:
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

- This system currently requires a Python client to be connected to the board over the network. The client can be found within the `netconn` directory. Instructions can be found in `netconn/README.md`.

- This demo system is integrated with the sDDF. Driver support will be inline with sDDF development.

- Additionally, found within the `toperf` directory is a tool to convert from the samples file outputted by the `netconn` tool to a `perf.data` file. This will allow us make use of existing infrastructure surrounding perf, such as `perf report` and flamegraph tools such as Mozilla Profiler.

# Demo System
A demo system can be found in the `example` directory. This includes a TCP echo server as well as two background dummy programs. This demo system is for the `odroidc4` and `maaxboard` platforms.

# How to use

## Controlling the profiler
In the `include/config.h` file, there is a #define called `CLIENT_CONFIG`. Set this value to whatever control method that you wish to use. 0 is using serial, 1 is using the network controller. 

### Serial Control
- Using this profiler over serial will result in the raw json samples being dumped to terminal.
- You can enter the following values into the terminal with the multiplexer set to hand characters to the profiler:
    - 1 - Start profiling
    - 2 - Stop profiling, dump all packets

This works on the `odroidc4` and `maaxboard`. Ensure that the Makefile is changed appropriately, and the right MMIO addresses and IRQ's are used in the system description.

### Network Control
More details can be found in the `netconn` directory.

## Converting to Perf
The serial control will output the raw JSON samples. The `netconn` tool will output the correctly formatted JSON file. Please see the `toperf/` directory for more information on using the `toperf` tool.
