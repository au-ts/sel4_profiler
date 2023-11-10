use core::fmt;
use std::mem;

use super::{file_section::FileSection, features::FeatureFlags, attributes::FileAttribute};

const PERF_MAGIC: u64 = 0x32454c4946524550;

#[repr(C)]
#[derive(Default, Debug)]
pub struct Header {
    // must be PERFFILE2 in little endian format
    pub magic: u64,

    // size of the header
    pub size: u64,

    // size of an attribute in attribute section
    pub attr_size: u64,

    // this section refers to particular attributes, which can be linked to events
    // in the data section
    pub attrs: FileSection,

    // the data section contains multiple events
    pub data: FileSection,

    // this section doesn't seem necessary for our usecase
    pub event_types: FileSection,

    // flags are used to extend the perf file with extra info
    // in our case only the first 64-bit variable is used
    pub flags: FeatureFlags,
    pub flags1: [u64; 3],
}

impl Header {
    // creates a header with default fields
    pub fn new() -> Header {
        let size = mem::size_of::<Header>() as u64;
        let attr_size = mem::size_of::<FileAttribute>() as u64;
        
        Header {
            magic: PERF_MAGIC,
            size,
            attr_size,

            // the attribute section occurs right after the header
            attrs: FileSection::new(size, attr_size),

            // the data section occurs right after the attribute section
            // but we don't have any attributes yet so just initialise
            // the size to 0
            data: FileSection::new(size + attr_size, 0),
            flags: FeatureFlags::NONE,
            ..Default::default()
        }
    }
}

impl fmt::Display for Header {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "Header:")?;
        writeln!(f, " magic: {:#018x}", self.magic)?;
        writeln!(f, " size: {}", self.size)?;
        writeln!(f, " attribute size: {}", self.attr_size)?;
        writeln!(f, " attribute section: {}", self.attrs)?;
        writeln!(f, " data section: {}", self.data)?;
        writeln!(f, " event types section: {}", self.event_types)?;
        writeln!(f, " feature flags: {:?}", self.flags)?;
        Ok(())
    }
}