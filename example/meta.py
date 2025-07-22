# Copyright 2025, UNSW
# SPDX-License-Identifier: BSD-2-Clause
import argparse
import struct
import random
from dataclasses import dataclass
from typing import List, Tuple
from sdfgen import SystemDescription, Sddf, DeviceTree
from importlib.metadata import version

assert version("sdfgen").split(".")[1] == "24", "Unexpected sdfgen version"

from sdfgen_helper import *
from config_structs import *

ProtectionDomain = SystemDescription.ProtectionDomain
MemoryRegion = SystemDescription.MemoryRegion
Map = SystemDescription.Map
Channel = SystemDescription.Channel


@dataclass
class Board:
    name: str
    arch: SystemDescription.Arch
    paddr_top: int
    serial: str
    timer: str
    ethernet: str

BOARDS: List[Board] = [
    Board(
        name="odroidc4",
        arch=SystemDescription.Arch.AARCH64,
        paddr_top=0x60000000,
        serial="soc/bus@ff800000/serial@3000",
        timer="soc/bus@ffd00000/watchdog@f0d0",
        ethernet="soc/ethernet@ff3f0000",
    ),
    Board(
        name="maaxboard",
        arch=SystemDescription.Arch.AARCH64,
        paddr_top=0x70000000,
        serial="soc@0/bus@30800000/serial@30860000",
        timer="soc@0/bus@30000000/timer@302d0000",
        ethernet="soc@0/bus@30800000/ethernet@30be0000",
    ),
]

def generate(sdf_file: str, output_dir: str, dtb: DeviceTree):
    uart_node = dtb.node(board.serial)
    assert uart_node is not None
    ethernet_node = dtb.node(board.ethernet)
    assert ethernet_node is not None
    timer_node = dtb.node(board.timer)
    assert timer_node is not None

    # Setup all the driver PD's
    timer_driver = ProtectionDomain("timer_driver", "timer_driver.elf", priority=101)
    timer_system = Sddf.Timer(sdf, timer_node, timer_driver)

    uart_driver = ProtectionDomain("uart_driver", "serial_driver.elf", priority=100)
    serial_virt_tx = ProtectionDomain("serial_virt_tx", "serial_virt_tx.elf", priority=96)
    serial_virt_rx = ProtectionDomain("serial_virt_rx", "serial_virt_rx.elf", priority=95)
    serial_system = Sddf.Serial(sdf, uart_node, uart_driver, serial_virt_tx, virt_rx=serial_virt_rx)

    ethernet_driver = ProtectionDomain("ethernet_driver", "eth_driver.elf", priority=101, budget=100, period=400)
    net_virt_tx = ProtectionDomain("net_virt_tx", "network_virt_tx.elf", priority=100, budget=20000)
    net_virt_rx = ProtectionDomain("net_virt_rx", "network_virt_rx.elf", priority=99)
    net_system = Sddf.Net(sdf, ethernet_node, ethernet_driver, net_virt_tx, net_virt_rx)

    prof_client = ProtectionDomain("prof_client", "prof_client.elf", priority=94)
    net_system.add_client(prof_client)
    serial_system.add_client(prof_client)
    timer_system.add_client(prof_client)
    prof_client_lwip = Sddf.Lwip(sdf, net_system, prof_client)

    echo_client = ProtectionDomain("echo", "echo.elf", priority=94, budget=20000)
    net_system.add_client(echo_client)
    serial_system.add_client(echo_client)
    timer_system.add_client(echo_client)
    echo_client_lwip = Sddf.Lwip(sdf, net_system, echo_client)

    profiler = ProtectionDomain("profiler", "profiler.elf", priority=105)

    # Connect the profiler to the profiler client
    profiler_ring_used = MemoryRegion(sdf, "profiler_ring_used", 0x200000)
    sdf.add_mr(profiler_ring_used)
    profiler_ring_used_prof_map = Map(profiler_ring_used, 0x8000000, perms="rw")
    profiler.add_map(profiler_ring_used_prof_map)
    profiler_ring_used_cli_map = Map(profiler_ring_used, 0x8000000, perms="rw")
    prof_client.add_map(profiler_ring_used_cli_map)

    profiler_ring_free = MemoryRegion(sdf, "profiler_ring_free", 0x200000)
    sdf.add_mr(profiler_ring_free)
    profiler_ring_free_prof_map = Map(profiler_ring_free, 0x8200000, perms="rw")
    profiler.add_map(profiler_ring_free_prof_map)
    profiler_ring_free_cli_map = Map(profiler_ring_free, 0x8200000)
    prof_client.add_map(profiler_ring_free_cli_map)

    profiler_mem = MemoryRegion(sdf, "profiler_mem", 0x1000000)
    sdf.add_mr(profiler_mem)
    profiler_mem_prof_map = Map(profiler_mem, 0x8600000, perms="rw")
    profiler.add_map(profiler_mem_prof_map)
    profiler_mem_cli_map = Map(profiler_mem, 0x8600000, perms="rw")
    prof_client.add_map(profiler_mem_cli_map)

    prof_ch = Channel(profiler, prof_client, pp_a=True)
    sdf.add_channel(prof_ch)

    profiler_conn = ProfilerConfig(RegionResource(profiler_ring_used_prof_map.vaddr, 0x200000),
                                   RegionResource(profiler_ring_free_prof_map.vaddr, 0x200000),
                                   RegionResource(profiler_mem_prof_map.vaddr, 0x1000000),
                                   prof_ch.pd_a_id)

    prof_client_conn = ProfilerConfig(RegionResource(profiler_ring_used_cli_map.vaddr, 0x200000),
                                   RegionResource(profiler_ring_free_cli_map.vaddr, 0x200000),
                                   RegionResource(profiler_mem_cli_map.vaddr, 0x1000000),
                                   prof_ch.pd_b_id)


    assert timer_system.connect()
    assert timer_system.serialise_config(output_dir)
    assert serial_system.connect()
    assert serial_system.serialise_config(output_dir)
    assert net_system.connect()
    assert net_system.serialise_config(output_dir)
    assert prof_client_lwip.connect()
    assert prof_client_lwip.serialise_config(output_dir)
    assert echo_client_lwip.connect()
    assert echo_client_lwip.serialise_config(output_dir)

    data_path = output_dir + "profiler_conn.data"
    with open(data_path, "wb+") as f:
        f.write(profiler_conn.serialise())
    data_path = output_dir + "prof_client_conn.data"
    with open(data_path, "wb+"):
        f.write(prof_client_conn.serialise)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--dtb", required=True)
    parser.add_argument("--board", required=True, choices=[b.name for b in BOARDS])
    parser.add_argument("--output", required=True)
    parser.add_argument("--sdf", required=True)

    args = parser.parse_args()

    board = next(filter(lambda b: b.name == args.board, BOARDS))

    sdf = SystemDescription(board.arch, board.paddr_top)
    sddf = Sddf(args.sddf)

    with open(args.dtb, "rb") as f:
        dtb = DeviceTree(f.read())

    generate(args.sdf, args.output, dtb)