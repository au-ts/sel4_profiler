use serde_with::{serde_as, DisplayFromStr};
use serde_aux::prelude::*;

pub const CALL_STACK_DEPTH: usize = 4;

#[repr(C)]
#[serde_as]
#[derive(Debug, serde::Serialize, serde::Deserialize)]
pub struct Sel4Sample {
    // instruction pointer
    #[serde(deserialize_with = "deserialize_number_from_string")]
    pub ip: u64,

    // protection domain id
    pub pid: u32,

    // timestamp of when the sample occured
    #[serde(deserialize_with = "deserialize_number_from_string")]
    pub time: u64,

    // cpu affinity - which cpu is being used
    pub cpu: u32,

    // number of events per sample
    #[serde(deserialize_with = "deserialize_number_from_string")]
    pub period: u64,

    // call stack - provides a trace of addresses for functions called
    #[serde_as(as = "[DisplayFromStr; CALL_STACK_DEPTH]")]
    pub ips: [u64; CALL_STACK_DEPTH],
}