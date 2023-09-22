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

RINGBUFFERDIR=libserialsharedringbuffer
XMODEMDIR=xmodem
UARTDIR=uart

BOARD_DIR := $(SEL4CP_SDK)/board/$(SEL4CP_BOARD)/$(SEL4CP_CONFIG)

IMAGES := profiler.elf client.elf uart.elf mux_rx.elf mux_tx.elf dummy_prog.elf dummy_prog2.elf 
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 -Wall  -Wno-unused-function -fno-omit-frame-pointer
LDFLAGS := -L$(BOARD_DIR)/lib -Llib
LIBS := -lsel4cp -Tsel4cp.ld -lc

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

CFLAGS += -I$(BOARD_DIR)/include \
	-Iinclude	\
	-I$(RINGBUFFERDIR)/include \
	-I$(BOARD_DIR)/include/sys \
	-I$(XMODEMDIR)/include \
	-I$(UARTDIR)/include \

UART_OBJS := uart/uart.o libserialsharedringbuffer/shared_ringbuffer.o
MUX_TX_OBJS := uart/mux_tx.o libserialsharedringbuffer/shared_ringbuffer.o
MUX_RX_OBJS := uart/mux_rx.o libserialsharedringbuffer/shared_ringbuffer.o
PROFILER_OBJS := profiler.o timer.o libserialsharedringbuffer/shared_ringbuffer.o
CLIENT_OBJS := client.o serial_server.o printf.o libserialsharedringbuffer/shared_ringbuffer.o xmodem/crc16.o xmodem/xmodem.o
DUMMY_PROG_OBJS := dummy_prog.o
DUMMY_PROG2_OBJS := dummy_prog2.o

all: directories $(IMAGE_FILE)

$(BUILD_DIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile
	$(AS) -g3 -mcpu=$(CPU) $< -o $@

$(BUILD_DIR)/profiler.elf: $(addprefix $(BUILD_DIR)/, $(PROFILER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart.elf: $(addprefix $(BUILD_DIR)/, $(UART_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/mux_rx.elf: $(addprefix $(BUILD_DIR)/, $(MUX_RX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/mux_tx.elf: $(addprefix $(BUILD_DIR)/, $(MUX_TX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog2.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG2_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/client.elf: $(addprefix $(BUILD_DIR)/, $(CLIENT_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) profiler.system
	$(SEL4CP_TOOL) profiler.system --search-path $(BUILD_DIR) --board $(SEL4CP_BOARD) --config $(SEL4CP_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

.PHONY: all depend compile clean

%.o:
	$(CC) $(CFLAGS) -c $(@:.o=.c) -o $@

#Make the Directories
directories:
	$(info $(shell mkdir -p $(BUILD_DIR)/libserialsharedringbuffer))	\
	$(info $(shell mkdir -p $(BUILD_DIR)/xmodem))	\
	$(info $(shell mkdir -p $(BUILD_DIR)/uart))	\

clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm


	