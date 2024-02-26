# toperf tool
- This repository serves as a tool to convert from seL4 sample packets to a format that perf can interpret. This is specifically for the PERFILE2.

- By utilising the pre-existing perf infrastructure we're able to take advantage of the many perf tools, as well as open source tools like flamegraph

## Installation
- This tool is written in the Rust Programming Language, and can be installed by following the link https://www.rust-lang.org/tools/install

## Building
- Run ``cargo build``. The binary will be built at ``/target/debug/toperf``

## Running
- Running the binary: ``./target/debug/toperf --samples-path <insert_sample_file_path_here> --build-dir <insert_build_directory_path_here>``
- Running through cargo: ``cargo run -- <-s/--samples_path insert_sample_file_path_here --build_dir insert_build_directory_path_here>``
- Optionally, you can include the `--print-summary` flag to print samples to console in a human readable format.
- Similar to ``perf record``, a ``perf.data`` file will be outputted in the current directory
- This ``perf.data`` file can be used to visualise samples recorded from an seL4 based system through commands such as ``perf report``, etc
- If you wish to use a tool such as Mozilla Profiler, you will have to generate a plain text version of the `perf report`. This can be done as follows: `perf script > perf.txt`.

## Details
- To use this tool, you must provide sample information in the following format

### Example json sample file:
```
{
    "elf_tcb_mappings": {
        "a.elf": 1
    },
    "samples": [
        {
            "ip": "2097304",
            "pid": 1,
            "time": "99088495",
            "period": "1200000",
            "ips": [
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0"
            ],
            "cpu": 0,
            "nr": "0"
        },{
            "ip": "2097340",
            "pid": 1,
            "time": "99096593",
            "period": "1200000",
            "ips": [
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0",
                "0"
            ],
            "cpu": 0,
            "nr": "0"
        }
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
    - ips: the callstack of the program at the time the sample was recorded. This is recorded to a maximum depth of 16.
    - nr: the number of instruction pointers within the callstack. 

## Mmap Events
Mmap events are required for symbol resolution on perf. We falsely create an mmap event, in which the elf path gets mapped
to a particular memory region corresponding to a pd. The actual mmap base we use
is important, and it must line up with whatever base is used in the elf. 

Currently, we set the base to 0x1f0000, which is the start address of our protection domains. 

## Credit

Created by Dominic Allas.
