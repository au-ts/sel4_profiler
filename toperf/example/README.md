# Example

In this directory we have an example of using the `toperf` tool. This was a TCP benchmark running on the `maaxboard`, with two dummy programs
running in the background. The samples are stored in `samples.json`, and the ELF files are in `/build/`.

## Converting to perf

Please run the following command in the top-level `toperf/` directory to generate the `perf.data` file from the samples.json.

```
cargo build
cd example/
../target/debug/toperf --samples-path samples.json --build-dir build
```

You should see a `perf.data` file has been generated.

## Using perf report

To see output using `perf` report please ensure that you have `perf` installed on your computer. Then you can simply use:

```
perf report
```

## Using Mozilla Profiler

A useful open-source tool that can also be used is the Mozilla profiler. To make use of these tools you will have to convert the `perf.data` file to a text file. This can be done as follows:

```
perf script > <your_file.txt>
```

You can upload this to https://profiler.firefox.com/ to view a call tree, flamegraph and more.
