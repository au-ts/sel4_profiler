// this file outlines structures related to the data section of the file

use crate::sample::{Sel4Sample, CALL_STACK_DEPTH};

use std::{mem, io::Write, fmt, ffi::CStr};

// Below are the constants for the 'misc' bits of a perf event header.
// Unused constants are left uncommented.

// const CPUMODE_UNKNOWN: u16 = 0 << 0;
const CPUMODE_KERNEL: u16 = 1 << 0;
const CPUMODE_USER: u16 = 2 << 0;
// const CPUMODE_HYPERVISOR: u16 = 3 << 0;
// const CPUMODE_GUEST_KERNEL: u16 = 4 << 0;
// const CPUMODE_GUEST_USER: u16 = 5 << 0;
// const PROC_MAP_PARSE_TIMEOUT: u16 = 1 << 12;
// const MMAP_DATA: u16 = 1 << 13;
const COMM_EXEC: u16 = 1 << 13;
// const FORK_EXEC: u16 = 1 << 13;
// const SWITCH_OUT: u16 = 1 << 13;
// const EXACT_IP: u16 = 1 << 14;
// const SWITCH_OUT_PREEMPT: u16 = 1 << 14;
// const MMAP_BUILD_ID: u16 = 1 << 14;
// const EXT_RESERVED: u16 = 1 << 15;

const PATH_MAX: usize = 4096;
const COMM_MAX: usize = 32;

fn align_up(address: usize, size: usize) -> usize {
    let mask = size - 1;
    (address + mask) & !mask
}

#[repr(C)]
#[derive(Debug)]
pub struct EventHeader {
    pub event_type: EventType,

    // indicates some miscellaneous information
    pub misc: u16,

    // the size of the record including the header
    pub size: u16,
}

#[repr(u32)]
#[derive(Debug)]
pub enum EventType {
    Mmap = 1,
    Lost = 2,
    Comm = 3,
    Exit = 4,
    Throttle = 5,
    Unthrottle = 6,
    Fork = 7,
    Read = 8,
    Sample = 9,
    Mmap2 = 10,
    Aux = 11,
    ItraceStart = 12,
    LostSamples = 13,
    Switch = 14,
    SwitchCpuWide = 15,
    Namespaces = 16,
    Ksymbol = 17,
    BpfEvent = 18,
    Cgroup = 19,
    TextPoke = 20,
    AuxOutputHwId = 21,
}

#[repr(C)]
#[derive(Debug)]
pub struct SampleIpCallchain {
    pub nr: u64,
    // TODO: it is a bit less than ideal for us to hard-code this
    // to CALL_STACK_DEPTH. Ideally it would be a run-time value.
    pub ips: [u64; CALL_STACK_DEPTH],
}

#[repr(C)]
#[derive(Debug)]
pub struct SampleEvent {
    header: EventHeader,

    // the perf structure uses constants such as
    // PERF_SAMPLE_IDENTIFIER, etc to optionally allow
    // fields in the sample
    // this means we can just support a basic amount of sample info
    pub ip: u64,
    pub pid: u32,
    pub tid: u32,
    pub time: u64,
    pub cpu: u32,
    pub period: u64,
    pub callchain: SampleIpCallchain,
}

impl SampleEvent {
    pub fn new(sample: Sel4Sample) -> Self {
        let header = EventHeader {
            event_type: EventType::Sample,
            misc: CPUMODE_USER,
            size: mem::size_of::<SampleEvent>() as u16,
        };

        // TODO: check that sample.ips.len matches CALL_STACK_DEPTH
        let callchain = SampleIpCallchain {
            nr: sample.ips.len() as u64,
            ips: sample.ips,
        };

        SampleEvent {
            header,
            ip: sample.ip,
            pid: sample.pd,
            tid: sample.pd,
            time: sample.timestamp,
            cpu: sample.cpu,
            period: sample.period,
            callchain: callchain,
        }
    }
}

impl fmt::Display for SampleEvent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "SampleEvent:")?;
        writeln!(f, " EventHeader:")?;
        writeln!(f, "  event_type: {:?}", self.header.event_type)?;
        writeln!(f, "  misc: {:#06x}", self.header.misc)?;
        writeln!(f, "  size: {}", self.header.size)?;
        writeln!(f, " ip: {:#018x}", self.ip)?;
        writeln!(f, " pid: {}", self.pid)?;
        writeln!(f, " tid: {}", self.tid)?;
        writeln!(f, " time: {}", self.time)?;
        writeln!(f, " cpu: {}", self.cpu)?;
        writeln!(f, " period: {}", self.period)?;
        writeln!(f, " callchain:")?;
        writeln!(f, "  nr: {}", self.callchain.nr)?;
        writeln!(f, "  ips: [")?;
        for ip in self.callchain.ips {
            writeln!(f, "    {:#018x},", ip)?;
        }
        writeln!(f, "  ]")?;
        Ok(()) 
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct CommEvent {
    header: EventHeader,
    pid: u32,
    tid: u32,
    comm: [u8; COMM_MAX],
}

impl CommEvent {
    pub fn new(pid: u32, application: &str) -> CommEvent {
        let header = EventHeader {
            event_type: EventType::Comm,
            misc: CPUMODE_KERNEL | COMM_EXEC,
            size: mem::size_of::<CommEvent>() as u16,
        };

        let mut comm = [0; COMM_MAX];
        fill_from_str(&mut comm, application);
        comm[COMM_MAX - 1] = 0;

        CommEvent {
            header,
            pid,
            tid: pid,
            comm,
        }
    }
}

impl fmt::Display for CommEvent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "CommEvent:")?;
        writeln!(f, " EventHeader:")?;
        writeln!(f, "  event_type: {:?}", self.header.event_type)?;
        writeln!(f, "  misc: {:#06x}", self.header.misc)?;
        writeln!(f, "  size: {}", self.header.size)?;
        writeln!(f, " pid: {}", self.pid)?;
        writeln!(f, " tid: {}", self.tid)?;
        
        let cstr = CStr::from_bytes_until_nul(&self.comm).expect("invalid cstr");
        writeln!(f, " comm: {}", cstr.to_str().unwrap())?;
        Ok(())
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct MmapEvent {
    pub header: EventHeader,
    pid: u32,
    tid: u32,
    start: u64,
    len: u64,
    pgoff: u64,
    filename: [u8; PATH_MAX],
}

impl MmapEvent {
    pub fn new(pid: u32, application: &str) -> MmapEvent {
        let application_size = align_up(application.len() + 1, 8);
        let header = EventHeader {
            event_type: EventType::Mmap,
            misc: CPUMODE_USER,
            size: ((mem::size_of::<MmapEvent>() - PATH_MAX + application_size) + 0x10) as u16,
        };

        let mut filename: [u8; PATH_MAX] = [0; PATH_MAX];
        fill_from_str(&mut filename, application);
        filename[PATH_MAX - 1] = 0;

        MmapEvent {
            header,
            pid,
            tid: pid,
            start: 2031616,
            len: 52768000,
            pgoff: 0,
            filename,
        }
    }
}

impl fmt::Display for MmapEvent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "MmapEvent:")?;
        writeln!(f, " EventHeader:")?;
        writeln!(f, "  event_type: {:?}", self.header.event_type)?;
        writeln!(f, "  misc: {:#06x}", self.header.misc)?;
        writeln!(f, "  size: {}", self.header.size)?;
        writeln!(f, " pid: {}", self.pid)?;
        writeln!(f, " tid: {}", self.tid)?;
        writeln!(f, " start: {:#018x}", self.start)?;
        writeln!(f, " len: {:#018x}", self.len)?;
        writeln!(f, " pgoff: {:#018x}", self.pgoff)?;
        
        let cstr = CStr::from_bytes_until_nul(&self.filename).expect("invalid cstr");
        writeln!(f, " filename: {}", cstr.to_str().unwrap())?;
        Ok(())
    }
}

fn fill_from_str(mut bytes: &mut [u8], s: &str) {
    bytes.write(s.as_bytes()).unwrap();
}