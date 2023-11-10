use serde::{Serialize, Deserialize};

pub const CALL_STACK_DEPTH: usize = 10;

#[repr(C)]
#[derive(Debug, Serialize, Deserialize)]
pub struct Sel4Sample {
    // instruction pointer
    pub ip: u64,

    // protection domain id
    pub pd: u32,

    // timestamp of when the sample occured
    pub timestamp: u64,

    // cpu affinity - which cpu is being used
    pub cpu: u32,

    // number of events per sample
    pub period: u64,

    // // call stack - provides a trace of addresses for functions called
    // pub ips: [u64; CALL_STACK_DEPTH],
}