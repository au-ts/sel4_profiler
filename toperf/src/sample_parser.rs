use std::{fs, io, path::Path, collections::HashMap};

use serde::{Serialize, Deserialize};

use crate::sample::Sel4Sample;

#[derive(Debug, Serialize, Deserialize)]
pub struct SamplesFile {
    pub pd_mappings: HashMap<String, u32>,
    pub samples: Vec<Sel4Sample>,
}

pub fn parse_samples<P: AsRef<Path>>(path: P) -> io::Result<SamplesFile> {
    let json_string = fs::read_to_string(path)?;
    let samples_file: SamplesFile = serde_json::from_str(&json_string)?;
    Ok(samples_file)
}