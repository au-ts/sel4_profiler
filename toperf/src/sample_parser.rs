use std::{fs, io, path::Path, collections::HashMap};

use serde::{Serialize, Deserialize};

use crate::sample::Sel4Sample;

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct RawSamplesFile {
    pub samples: Vec<Sel4Sample>,
}

#[derive(Debug, Serialize, Deserialize, Default)]
pub struct SampleHeaderFile {
    pub elf_tcb_mappings: HashMap<String, u32>,
}

#[derive(Debug, Default)]
pub struct SamplesFile {
    pub elf_tcb_mappings: SampleHeaderFile,
    pub samples: RawSamplesFile,
}

pub fn parse_samples<P: AsRef<Path>>(sample_path: P, sample_header_path: P) -> io::Result<SamplesFile> {

    let mut samples_file: SamplesFile = SamplesFile::default();

    let sample_json = fs::read_to_string(sample_path)?;
    let sample_header_json = fs::read_to_string(sample_header_path)?;

    samples_file.samples = serde_json::from_str(&sample_json)?;
    samples_file.elf_tcb_mappings = serde_json::from_str(&sample_header_json)?;
    Ok(samples_file)
}
