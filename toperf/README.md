# toperf tool
- This repository serves as a tool to convert from seL4 sample packets to a format that perf can interpret

- By utilising the pre-existing perf infrastructure we're able to take advantage of the many perf tools, as well as open source tools like flamegraph

## Installation
- This tool is written in the Rust Programming Language, and can be installed by following the link https://www.rust-lang.org/tools/install

## Building
- Run ``cargo build``. The binary will be built at ``/target/debug/toperf``

## Running
- Running the binary: ``./target/debug/toperf <-s/--samples_path insert_sample_file_path_here>``
- Running through cargo: ``cargo run -- <-s/--samples_path insert_sample_file_path_here>``
- Similar to ``perf record``, a ``perf.data`` file will be outputted in the current directory
- This ``perf.data`` file can be used to visualise samples recorded from an seL4 based system through commands such as ``perf report``, etc

## Details
- To use this tool, you must provide sample information in the following format

### Example json sample file:
```
{
    "elf_tcb_mappings": {
        "a.elf": 0
    },
    "samples": [
        {
            "ip": 0,
            "pd": 0,
            "timestamp": 0,
            "cpu": 0,
            "period": 300
        },
        {
            "ip": 4,
            "pd": 0,
            "timestamp": 20,
            "cpu": 0,
            "period": 300
        },
        {
            "ip": 8,
            "pd": 0,
            "timestamp": 40,
            "cpu": 0,
            "period": 300
        },
    ]
}
```

- elf_tcb_mappings: an object that maps elf paths to pds expected by perf
- samples: contains a list of objects with the following fields:
    - ip: the instruction pointer of the cpu when the sample was recorded
    - pd: the protection domain that the sample was recorded in
    - timestamp: the time of when the sample was recorded since the program started running
    - cpu: the id of the cpu that the sample was recorded on
    - period: refers to how often sample data is sampled

## Credit

Created by Dominic Allas.
