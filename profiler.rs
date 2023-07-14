#![no_std]
#![no_main]
#![feature(never_type)]
// @ivanv: this is only here because we shared ring buffer is not a seperate crate yet
#![feature(core_intrinsics)]

use sel4cp::{protection_domain, debug_println, Handler, Channel,};
use core::intrinsics;
use core::arch::asm;

const REGION_SIZE: usize = 0x200_000;

const SERIAL_TX: Channel = Channel::new(9);
const SERIAL_RX: Channel = Channel::new(11);
const APPLICATION: Channel = Channel::new(5);

// Global arrays and event bit string
thread_local!(static ACTIVE_COUNTERS: u32 = (1 << 31));

// Need to add in the C print functions

// From Ivan's sDDF port. His note - "fucking stupid"
fn void() {}

#[protection_domain]
fn init() -> ProfilerHandler{

    unsafe {enable_cycle_counter();};

    ProfilerHandler {
        initialised: true
    }
}

unsafe fn enable_cycle_counter() {
    debug_println!("PROFILER_RS: WE ARE ENABLING PMU IN RUST");

    let mut init_cnt: u64 = 0xffffffffffffffff - 12000000000;
    let mut val: u64 = 0;
    let enable_cnt: u32 = (1 << 31);

    asm! (
        "mrs {0}, pmcr_el0",
        out(reg) val,
    );

	val |= (1) | (1 << 2);

    asm! (
      "isb", 
      "msr pmcr_el0, {0}",
      in(reg) val, 
    );

    asm! (
        "msr pmccntr_el0, {0}",
        in(reg) init_cnt,
    );

    asm! (
        "msr pmcntenset_el0, {0}",
        in(reg) enable_cnt,
    );


}

// The following functions are to allow configuration of the event counters
fn configure_cnt0(event: u32, val: u64) {
    asm! (
        "isb", 
        "msr pmevtyper0_el0, {0}", 
        in(reg) event,
    );

    asm! (
        "msr pmevcntr0_el0, {0}",
        in(reg) val,
    );
}

fn configure_cnt1(event: u32, val: u64) {
    asm! (
        "isb", 
        "msr pmevtyper1_el0, {0}", 
        in(reg) event,
    );

    asm! (
        "msr pmevcntr1_el0, {0}",
        in(reg) val,
    );
}

fn configure_cnt2(event: u32, val: u64) {
    asm! (
        "isb", 
        "msr pmevtyper2_el0, {0}", 
        in(reg) event,
    );

    asm! (
        "msr pmevcntr2_el0, {0}",
        in(reg) val,
    );
}

fn configure_cnt3(event: u32, val: u64) {
    asm! (
        "isb", 
        "msr pmevtyper3_el0, {0}", 
        in(reg) event,
    );

    asm! (
        "msr pmevcntr3_el0, {0}",
        in(reg) val,
    );
}

fn configure_cnt4(event: u32, val: u64) {
    asm! (
        "isb", 
        "msr pmevtyper4_el0, {0}", 
        in(reg) event,
    );

    asm! (
        "msr pmevcntr4_el0, {0}",
        in(reg) val,
    );
}

fn configure_cnt5(event: u32, val: u64) {
    asm! (
        "isb", 
        "msr pmevtyper5_el0, {0}", 
        in(reg) event,
    );

    asm! (
        "msr pmevcntr5_el0, {0}",
        in(reg) val,
    );
}

struct ProfilerHandler {
    initialised: bool,
}

impl Handler for ProfilerHandler {
    type Error = !;

    fn notified(&mut self, channel: Channel) -> Result<(), Self::Error> {
        Ok(())
    }
}