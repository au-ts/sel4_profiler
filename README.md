# seL4 Profiler
This is a framework for profiling on-top of seL4. It is a statistical profiler that can be configured to 
sample based on different events and frequencies. There is a sample client that can dump packets either 
over serial or the network. 

Currently we have support for the imx8mm soc.

# Installation

- We require a special kernel configuration. The kernel version can be found at: https://github.com/Kswin01/seL4/tree/microkit-dev-profiler.
- We have tested with this version of microkit: https://github.com/Ivan-Velickovic/microkit.

# Building
- Ensure that you have either downloaded the appropriate microkit SDK, or built from source. Instructions for building microkit can be found in the microkit repo.
- Use the following Make command:
```
make BUILD_DIR=<build_dir> MICROKIT_SDK=<path_to_sdk> MICROKIT_BOARD=<board> MICROKIT_CONFIG=benchmark
```

# Using

- This demo system currently requires a Python client to be connected to the board over the network. The client can be found at: https://github.com/Kswin01/seL4-Profiler-Linux-Network-Host}.
- Additionally, this demo is for the imx8mm.
- When the board first starts running, an IP address will be printed to the terminal. Please modify the current IP address in the Python script.
- Connect the client to the board by running the `connect` command on the Python client. You may need to restart the board here.
- To start profiling, enter the `start` command.
- To pause profiling, enter the `stop` command.
- To finish profiling, enter the `exit` command. 