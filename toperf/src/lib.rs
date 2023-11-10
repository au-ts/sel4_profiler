pub mod perf;
pub mod sample;
pub mod sample_parser;

use std::{mem, slice};

pub fn as_raw_bytes<T>(data: &T) -> &[u8] {
    let ptr = (data as *const T) as *const u8;
    let size = mem::size_of::<T>();
    let bytes = unsafe { slice::from_raw_parts(ptr, size) };
    bytes
}

#[cfg(test)]
mod tests {
    use crate::as_raw_bytes;
    use super::*;

    #[repr(C)]
    struct TestStruct {
        a: u8,
        b: u32,
        c: u16,
    }

    #[test]
    fn test_as_raw_bytes() {
        let input = TestStruct {
            a: 0x01,
            b: 0xdeadbeef,
            c: 0x1234,
        };

        let bytes = as_raw_bytes(&input);
        assert_eq!(mem::size_of::<TestStruct>(), 7);
        assert_eq!(bytes, &[0x01, 0xef, 0xbe, 0xad, 0xde, 0x34, 0x12]);
    }
}