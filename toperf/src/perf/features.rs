// this file outlines structures related to the features section of the file

use bitflags::bitflags;

bitflags! {
    #[repr(C)]
    #[derive(Default, Debug)]
    pub struct FeatureFlags: u64 {
        const FIRST_FEATURE = 1 << 0;
        const TRACING_DATA = 1 << 0;
        const BUILD_ID = 1 << 1;
        const HOSTNAME = 1 << 2;
        const OSRELEASE = 1 << 3;
        const VERSION = 1 << 4;
        const ARCH = 1 << 5;
        const NRCPUS = 1 << 6;
        const CPUDESC = 1 << 7;
        const CPUID = 1 << 8;
        const TOTAL_MEM = 1 << 9;
        const CMDLINE = 1 << 10;
        const EVENT_DESC = 1 << 11;
        const CPU_TOPOLOGY = 1 << 12;
        const NUMA_TOPOLOGY = 1 << 13;
        const BRANCH_STACK = 1 << 14;
        const PMU_MAPPINGS = 1 << 15;
        const GROUP_DESC = 1 << 16;
        const AUXTRACE = 1 << 17;
        const STAT = 1 << 18;
        const CACHE = 1 << 19;
        const SAMPLE_TIME = 1 << 20;
        const MEM_TOPOLOGY = 1 << 21;
        const CLOCKID = 1 << 22;
        const DIR_FORMAT = 1 << 23;
        const BPF_PROG_INFO = 1 << 24;
        const BPF_BTF = 1 << 25;
        const COMPRESSED = 1 << 26;
        const CPU_PMU_CAPS = 1 << 27;
        const CLOCK_DATA = 1 << 28;
        const HYBRID_TOPOLOGY = 1 << 29;
        const PMU_CAPS = 1 << 30;
        const LAST_FEATURE = 1 << 31;
        const NONE = 1 << 32;
    }
}