// this file outlines structures related to the attributes section of the file

use std::fmt;

use super::file_section::FileSection;

use bitflags::bitflags;

#[repr(C)]
#[derive(Default, Debug)]
pub struct FileAttribute {
    pub attr: EventAttribute,

    // points to a file section which contains an array of u64 ids
    // i assume these ids are for the events which are linked to this attribute
    pub ids: FileSection,
}

impl fmt::Display for FileAttribute {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "FileAttribute:")?;
        writeln!(f, " EventAttribute:")?;
        writeln!(f, "  event_type: {:?}", self.attr.event_type)?;
        writeln!(f, "  size: {}", self.attr.size)?;
        writeln!(f, "  sample_period_or_freq: {}", self.attr.sample_period_or_freq)?;
        writeln!(f, "  sample_type: {:?}", self.attr.sample_type)?;
        writeln!(f, "  attr_flags: {:?}", self.attr.attr_flags)?;
        writeln!(f, " ids: {}", self.ids)?;
        Ok(())
    }
}

#[repr(C)]
#[derive(Default, Debug)]
pub struct EventAttribute {
    // major type: hardware/software/tracepoint/etc
    pub event_type: EventType,

    // size of the attribute
    pub size: u32,

    // TODO: figure out what this is for
    pub config: u64,

    pub sample_period_or_freq: u64,
    pub sample_type: SampleType,
    pub read_format: ReadFormat,
    pub attr_flags: AttributeFlags,

    // wakeup every n events of bytes before wakeup
    pub wakeup_events_or_watermark : u32,

    pub bp_type: u32,
    pub bp_addr_or_config1: u64,
    pub bp_len_or_config2: u64,

    // doesn't seem to be useful
    pub branch_sample_type: u64,

    // defines set of user regs to dump on samples
    pub sample_regs_user: u64,

    // defines size of the user stack to dump on sampes
    pub sample_stack_user: u32,

    pub clockid: i32,

    // defines set of regs to dump for each sample
    // precise = 0: pmu interrupt
    // precise > 0: sampled instruction
    pub sample_regs_intr: u64,

    // wakeup watermark for aux area
    pub aux_watermark: u32,

    pub sample_max_stack: u16,
    pub reserved2: u16,
}

#[repr(u32)]
#[derive(Debug)]
pub enum EventType {
    Hardware = 0,
    Software = 1,
    Tracepoint = 2,
    HwCache = 3,
    Raw = 4,
    Breakpoint = 5,
}

impl Default for EventType {
    fn default() -> Self {
        EventType::Hardware
    }
}

bitflags! {
    #[repr(C)]
    #[derive(Default, Debug)]
    pub struct SampleType: u64 {
        const IP = 1 << 0;
        const TID = 1 << 1;
        const TIME = 1 << 2;
        const ADDR = 1 << 3;
        const READ = 1 << 4;
        const CALLCHAIN = 1 << 5;
        const ID = 1 << 6;
        const CPU = 1 << 7;
        const PERIOD = 1 << 8;
        const STREAM_ID = 1 << 9;
        const RAW = 1 << 10;
        const BRANCH_STACK = 1 << 11;
        const REGS_USER = 1 << 12;
        const STACK_USER = 1 << 13;
        const WEIGHT = 1 << 14;
        const DATA_SRC = 1 << 15;
        const IDENTIFIER = 1 << 16;
        const TRANSACTION = 1 << 17;
        const REGS_INTR = 1 << 18;
        const PHY_ADDR = 1 << 19;
        const AUX = 1 << 20;
        const CGROUP = 1 << 21;
        const DATA_PAGE_SIZE = 1 << 22;
        const CODE_PAGE_SIZE = 1 << 23;
        const WEIGHT_STRUCT = 1 << 24;
    }
}


bitflags! {
    #[repr(C)]
    #[derive(Default, Debug)]
    pub struct ReadFormat: u64 {
        const TOTAL_TIME_ENABLED = 1 << 0;
        const TOTAL_TIME_RUNNING = 1 << 1;
        const ID = 1 << 2;
        const GROUP = 1 << 3;
        const LOST = 1 << 4;
    }
}

bitflags! {
    #[repr(C)]
    #[derive(Default, Debug)]
    pub struct AttributeFlags : u64 {
        const DISABLED = 1 << 0; // off by default
        const INHERIT = 1 << 1; // children inherit it
        const PINNED = 1 << 2; // must always be on pmu
        const EXCLUSIVE = 1 << 3; // only group on pmu
        const EXCLUDE_USER = 1 << 4; // don't count user
        const EXCLUDE_KERNEL = 1 << 5; // ditto kernel
        const EXCLUDE_HV = 1 << 6; // ditto hypervisor
        const EXCLUDE_IDLE = 1 << 7; // don't count when idle
        const MMAP = 1 << 8; // include mmap data
        const COMM = 1 << 9; // include comm data
        const FREQ = 1 << 10; // use freq, not period
        const INHERIT_STAT = 1 << 11; // per task counts
        const ENABLE_ON_EXEC = 1 << 12; // next exec enables
        const TASK = 1 << 13; // trace fork/exit
        const WATERMARK = 1 << 14; // wakeup_watermark

        // precise_ip:
        // 0 - SAMPLE_IP can have arbitrary skid
        // 1 - SAMPLE_IP must have constant skid
        // 2 - SAMPLE_IP requested to have 0 skid
        // 3 - SAMPLE_IP must have 0 skid
        const PRECISE_IP1 = 1 << 15;
        const PRECISE_IP2 = 1 << 16;

        const MMAP_DATA = 1 << 17; // non-exec mmap data
        const SAMPLE_ID_ALL = 1 << 18; // sample_type all events
        const EXCLUDE_HOST = 1 << 19; // don't count in host
        const EXCLUDE_GUEST = 1 << 20; // don't count in guest
        const EXCLUDE_CALLCHAIN_KERNEL = 1 << 21; // exclude kernel callchains
        const EXCLUDE_CALLCHAIN_USER = 1 << 22; // exclude user callchains
        const MMAP2 = 1 << 23; // include mmap with inode data
        const COMM_EXEC = 1 << 24; // flag comm events that are due to an exec
        const USE_CLOCKID = 1 << 25; // use clockid for time fields
        const CONTEXT_SWITCH = 1 << 26; // context switch data
        const WRITE_BACKWARD = 1 << 27; // write ring buffer from end to beginning
        const NAMESPACES = 1 << 28; // include namespaces data
        const KSYMBOL = 1 << 29; // include ksymbol events
        const BPF_EVENT = 1 << 30; // include bpf events
        const AUX_OUTPUT = 1 << 31; // generate aux records instead of events
        const CGROUP = 1 << 32; // include cgroup events
        const TEXT_POKE = 1 << 33; // include text poke events
        const BUILD_ID = 1 << 34; // use build id in mmap2 events
        const INHERIT_THREAD = 1 << 35; // children only inherit if cloned with CLONE_THREAD
        const REMOVE_ON_EXEC = 1 << 36; // event is removed from task on exec
        const SIGTRAP = 1 << 37; // send synchronous SIGTRAP on event
    }
}