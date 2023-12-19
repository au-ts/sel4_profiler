use std::{fs::File, path::Path};

use toperf::{perf::PerfFile, sample_parser};

use clap::Parser;

#[derive(Parser, Debug)]
struct Args {
    /// Path to the output perf data file
    #[arg(short, long, default_value = "perf.data")]
    output: String,
    /// Path of the sample file in JSON format
    #[arg(short, long)]
    samples_path: String,
    /// Path to build directory containing ELFs referenced in the samples
    #[arg(short, long)]
    build_dir: String,
    /// Print a summary of the samples parsed to stdout
    #[arg(short, long, action=clap::ArgAction::SetTrue)]
    print_summary: bool,
}

fn main() -> std::io::Result<()> {
    let args = Args::parse();

    let mut file = File::create(args.output)?;
    let mut perf_file = PerfFile::new()?;

    // add samples from samples file
    let samples_file = sample_parser::parse_samples(args.samples_path)?;

    // create perf events according to the samples file
    // TODO: check that build_dir actually exists
    // TODO: check that the ELF path actually exists
    let build_dir = args.build_dir;
    for (application, pid) in samples_file.elf_tcb_mappings {
        let elf_path_name = format!("{build_dir}/{application}");
        let elf_path = Path::new(&elf_path_name);

        let filename = elf_path.file_name().unwrap().to_str().unwrap();
        perf_file.create_comm_event(pid, filename);
        perf_file.create_mmap_event(pid, &application);
    }

    for sample in samples_file.samples {
        perf_file.create_sample_event(sample);
    }

    if args.print_summary {
        perf_file.print_summary();
    }
    perf_file.dump_to_file(&mut file)?;
    Ok(())
}
