use std::fmt;

#[repr(C)]
#[derive(Default, Clone, Copy, Debug)]
pub struct FileSection {
    // an offset into the file for where this section starts
    pub offset: u64,

    // the size of the section
    pub size: u64,
}

impl FileSection {
    pub fn new(offset: u64, size: u64) -> Self {
        FileSection {
            offset,
            size,
        }
    }
}

impl fmt::Display for FileSection {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:#016x}..{:#016x}", self.offset, self.offset + self.size)
    }
}