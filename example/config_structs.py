from typing import List
from ctypes import *

# Macros:

# Struct Classes
# region_resource_t
class RegionResourceStruct(LittleEndianStructure):
                 # C type: void*
    _fields_ = [("vaddr", c_uint64),
                 # C type: uint64_t
                 ("size", c_uint64)]

# profiler_config_t
class ProfilerConfigStruct(LittleEndianStructure):
                 # C type: region_resource_t
    _fields_ = [("profiler_ring_used", RegionResourceStruct),
                 # C type: region_resource_t
                 ("profiler_ring_free", RegionResourceStruct),
                 # C type: region_resource_t
                 ("profiler_mem", RegionResourceStruct),
                 # C type: uint8_t
                 ("ch", c_uint8)]


class Serializable():
    def serialise(self):
        return bytes(self.to_struct())

class RegionResource(Serializable):
    def __init__(self, vaddr: int, size: int):
        self.vaddr = vaddr
        self.size = size
        self.section_name = "region_resource"

    def to_struct(self) -> RegionResourceStruct:
        vaddr_arg = c_uint64() if self.vaddr is None else self.vaddr
        size_arg = c_uint64() if self.size is None else self.size
        return RegionResourceStruct(vaddr_arg, 
                                    size_arg)

class ProfilerConfig(Serializable):
    def __init__(self, profiler_ring_used: RegionResource, profiler_ring_free: RegionResource, profiler_mem: RegionResource, ch: int):
        self.profiler_ring_used = profiler_ring_used
        self.profiler_ring_free = profiler_ring_free
        self.profiler_mem = profiler_mem
        self.ch = ch
        self.section_name = "profiler_config"

    def to_struct(self) -> ProfilerConfigStruct:
        profiler_ring_used_arg = RegionResourceStruct() if self.profiler_ring_used is None else self.profiler_ring_used.to_struct()
        profiler_ring_free_arg = RegionResourceStruct() if self.profiler_ring_free is None else self.profiler_ring_free.to_struct()
        profiler_mem_arg = RegionResourceStruct() if self.profiler_mem is None else self.profiler_mem.to_struct()
        ch_arg = c_uint8() if self.ch is None else self.ch
        return ProfilerConfigStruct(profiler_ring_used_arg, 
                                    profiler_ring_free_arg, 
                                    profiler_mem_arg, 
                                    ch_arg)

