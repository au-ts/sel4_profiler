#![no_std]
#![no_main]
#![feature(never_type)]
// @ivanv: this is only here because we shared ring buffer is not a seperate crate yet
#![feature(core_intrinsics)]

use sel4cp::{protection_domain, debug_println, Handler, Channel,};
use sel4cp::memory_region::{
    declare_memory_region, ReadWrite,
};
use core::intrinsics;

const REGION_SIZE: usize = 0x200_000;

const SERIAL_TX: Channel = Channel::new(9);
const SERIAL_RX: Channel = Channel::new(11);
const APPLICATION: Channel = Channel::new(5);

