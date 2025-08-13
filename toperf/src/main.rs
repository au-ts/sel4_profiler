use std::{fs::File, path::Path};

use toperf::{perf::PerfFile, sample_parser};

use clap::Parser;

use std::process;

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

    // Create a path to the samples header file, which will be in
    // the build directory and named 'sample_header.json'
    let mut sample_header_path = args.build_dir.clone();
    sample_header_path.push_str("/sample_header.json");

    // add samples from samples file
    let samples_file = sample_parser::parse_samples(args.samples_path, sample_header_path)?;

    // create perf events according to the samples file
    // TODO: check that build_dir actually exists
    // TODO: check that the ELF path actually exists
    let build_dir = args.build_dir;
    for (application, pid) in samples_file.elf_tcb_mappings.elf_tcb_mappings {
        let elf_path_name = format!("{build_dir}/{application}");
        let elf_path = Path::new(&elf_path_name);
        // Check if elf path exists
        if !elf_path.exists() {
            println!("Elf path: {elf_path_name} does not exist!");
            process::exit(1);
        }

        let filename = elf_path.file_name().unwrap().to_str().unwrap();

        perf_file.create_comm_event(pid, filename);
        perf_file.create_mmap_event(pid, &elf_path_name);
    }

    for sample in samples_file.samples.samples {
        perf_file.create_sample_event(sample);
    }

    if args.print_summary {
        perf_file.print_summary();
    }
    perf_file.dump_to_file(&mut file)?;
    Ok(())
}
