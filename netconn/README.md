# Network Host

This is a basic python program to connect to the seL4 Profiling Framework,
and collects samples over the network in the form of protocol buffers to store in JSON format.
These samples will be stored in the `samples.json` file.

## Setup

This tool depends on `protobuf` Python package which can be installed with:
```sh
pip3 install protobuf
```

## Usage

```sh
python3 netconn.py --ip <TARGET_IP> --port <TARGET_PORT>
```

- Connect the client to the board by running the `connect` command on the Python client. You may need to restart the board here.
- To start profiling, enter the `start` command.
- To pause profiling, enter the `stop` command.
- To finish profiling, enter the `exit` command.

If you type `help` you will see all the possible commands.

