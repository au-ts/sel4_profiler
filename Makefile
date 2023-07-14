#
# Copyright 2022, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#

ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must be specified)
endif

ifeq ($(strip $(SEL4CP_SDK)),)
$(error SEL4CP_SDK must be specified)
endif

ifeq ($(strip $(SEL4CP_BOARD)),)
$(error SEL4CP_BOARD must be specified)
endif

ifeq ($(strip $(SEL4CP_CONFIG)),)
$(error SEL4CP_CONFIG must be specified)
endif

TOOLCHAIN := aarch64-none-elf

CPU := cortex-a55

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
SEL4CP_TOOL ?= $(SEL4CP_SDK)/bin/sel4cp

RINGBUFFERDIR=libsharedringbuffer

BOARD_DIR := $(SEL4CP_SDK)/board/$(SEL4CP_BOARD)/$(SEL4CP_CONFIG)

IMAGES := profiler.elf serial.elf dummy_prog.elf dummy_prog2.elf profiler_rs.elf
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 -Wall  -Wno-unused-function
LDFLAGS := -L$(BOARD_DIR)/lib -Llib
LIBS := -lsel4cp -Tsel4cp.ld -lc

########################
# Rust compilation stuff

RUST_TARGET_PATH := rust_target
RUST_SEL4CP_TARGET := aarch64-sel4cp-minimal
TARGET_DIR := $(BUILD_DIR)/target

SEL4CP_SDK_CONFIG := $(SEL4CP_SDK)/board/$(SEL4CP_BOARD)/$(SEL4CP_CONFIG)/config.json
SEL4CP_SDK_INCLUDE_DIR := $(SEL4CP_SDK)/board/$(SEL4CP_BOARD)/$(SEL4CP_CONFIG)/include

RUST_ENV := \
	RUST_TARGET_PATH=$(abspath $(RUST_TARGET_PATH)) \
	SEL4_CONFIG=$(abspath $(SEL4CP_SDK_CONFIG)) \
	SEL4_INCLUDE_DIRS=$(abspath $(SEL4CP_SDK_INCLUDE_DIR))

RUST_OPTIONS := \
	--locked \
	-Z unstable-options \
	-Z bindeps \
	-Z build-std=core,alloc,compiler_builtins \
	-Z build-std-features=compiler-builtins-mem \
	--target $(RUST_SEL4CP_TARGET) \
	--release \
	--target-dir $(abspath $(TARGET_DIR)) \

RUST_DOC_OPTIONS := $(RUST_OPTIONS)

RUST_BUILD_OPTIONS := \
	$(RUST_OPTIONS) \
	--out-dir $(BUILD_DIR)

# Rust compilation stuff ends
########################

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

CFLAGS += -I$(BOARD_DIR)/include \
	-Iinclude	\
	-I$(RINGBUFFERDIR)/include \

SERIAL_OBJS := serial.o libsharedringbuffer/shared_ringbuffer.o
PROFILER_OBJS := profiler.o serial_server.o printf.o libsharedringbuffer/shared_ringbuffer.o
DUMMY_PROG_OBJS := dummy_prog.o
DUMMY_PROG2_OBJS := dummy_prog2.o
all: directories $(IMAGE_FILE)

doc:
	$(RUST_ENV) \
		cargo doc $(RUST_DOC_OPTIONS) --open

$(BUILD_DIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile
	$(AS) -g3 -mcpu=$(CPU) $< -o $@

$(BUILD_DIR)/profiler.elf: $(addprefix $(BUILD_DIR)/, $(PROFILER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/serial.elf: $(addprefix $(BUILD_DIR)/, $(SERIAL_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog2.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG2_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/profiler_rs.elf: profiler.rs
	$(RUST_ENV) \
		cargo build $(RUST_BUILD_OPTIONS)

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) profiler.system
	$(SEL4CP_TOOL) profiler.system --search-path $(BUILD_DIR) --board $(SEL4CP_BOARD) --config $(SEL4CP_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

.PHONY: all depend compile clean

%.o:
	$(CC) $(CFLAGS) -c $(@:.o=.c) -o $@

#Make the Directories
directories:
	$(info $(shell mkdir -p $(BUILD_DIR)/libsharedringbuffer))	\

clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm


	