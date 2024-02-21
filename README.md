# seL4 Profiler
This is a framework for profiling on-top of seL4. It is a statistical profiler that can be configured to 
sample based on different events and frequencies. There is a sample client that can dump packets either 
over serial or the network. 

Currently we have support for the imx8mm soc.

# Installation

- We require a special kernel configuration. The kernel version can be found at: https://github.com/Kswin01/seL4/tree/microkit-dev-profiler-irq.
- We have tested with this version of microkit: https://github.com/Kswin01/sel4cp/tree/microkit_prof_dev.

# Building
- Ensure that you have either downloaded the appropriate microkit SDK, or built from source. Instructions for building microkit can be found in the microkit repo.
- Use the following Make command:
```
make BUILD_DIR=<build_dir> MICROKIT_SDK=<path_to_sdk> MICROKIT_BOARD=<board> MICROKIT_CONFIG=benchmark
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


