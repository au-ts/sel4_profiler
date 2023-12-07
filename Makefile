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

LWIP=network/ipstacks/lwip/src
UTIL=include/
ETH_RING_BUFFER=network/libethsharedringbuffer
ETHERNET_DRIVER=network/imx
TIMER_DRIVER=clock/imx
NETWORK_COMPONENTS=network/components

RINGBUFFERDIR=libserialsharedringbuffer
XMODEMDIR=xmodem
UARTDIR=uart
NANOPBDIR=nanopb

BOARD_DIR := $(SEL4CP_SDK)/board/$(SEL4CP_BOARD)/$(SEL4CP_CONFIG)

IMAGES := profiler.elf client.elf uart.elf uart_mux_rx.elf uart_mux_tx.elf dummy_prog.elf dummy_prog2.elf eth.elf eth_mux_rx.elf eth_mux_tx.elf eth_copy.elf arp.elf timer.elf
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g -O0 -Wall  -Wno-unused-function -fno-omit-frame-pointer
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
	-I$(LWIP)/include \
	-I$(LWIP)/include/ipv4 \
	-I$(RING_BUFFER)/include \
	-MD \
	-MP

# COREFILES, CORE4FILES: The minimum set of files needed for lwIP.
COREFILES=$(LWIP)/core/init.c \
	$(LWIP)/core/def.c \
	$(LWIP)/core/dns.c \
	$(LWIP)/core/inet_chksum.c \
	$(LWIP)/core/ip.c \
	$(LWIP)/core/mem.c \
	$(LWIP)/core/memp.c \
	$(LWIP)/core/netif.c \
	$(LWIP)/core/pbuf.c \
	$(LWIP)/core/raw.c \
	$(LWIP)/core/stats.c \
	$(LWIP)/core/sys.c \
	$(LWIP)/core/altcp.c \
	$(LWIP)/core/altcp_alloc.c \
	$(LWIP)/core/altcp_tcp.c \
	$(LWIP)/core/tcp.c \
	$(LWIP)/core/tcp_in.c \
	$(LWIP)/core/tcp_out.c \
	$(LWIP)/core/timeouts.c \
	$(LWIP)/core/udp.c

CORE4FILES=$(LWIP)/core/ipv4/autoip.c \
	$(LWIP)/core/ipv4/dhcp.c \
	$(LWIP)/core/ipv4/etharp.c \
	$(LWIP)/core/ipv4/icmp.c \
	$(LWIP)/core/ipv4/igmp.c \
	$(LWIP)/core/ipv4/ip4_frag.c \
	$(LWIP)/core/ipv4/ip4.c \
	$(LWIP)/core/ipv4/ip4_addr.c

# NETIFFILES: Files implementing various generic network interface functions
NETIFFILES=$(LWIP)/netif/ethernet.c

# LWIPFILES: All the above.
LWIPFILES=$(NETWORK_COMPONENTS)/lwip.c $(NETWORK_COMPONENTS)/lwip_timer.c cache.c $(COREFILES) $(CORE4FILES) $(NETIFFILES)
LWIP_OBJS := $(LWIPFILES:.c=.o) $(NETWORK_COMPONENTS)/lwip.o $(ETH_RING_BUFFER)/shared_ringbuffer.o $(NETWORK_COMPONENTS)/utilization_socket.o

UART_OBJS := uart/uart.o libserialsharedringbuffer/shared_ringbuffer.o
UART_MUX_TX_OBJS := uart/mux_tx.o libserialsharedringbuffer/shared_ringbuffer.o
UART_MUX_RX_OBJS := uart/mux_rx.o libserialsharedringbuffer/shared_ringbuffer.o
PROFILER_OBJS := profiler.o libserialsharedringbuffer/shared_ringbuffer.o
CLIENT_OBJS := client.o serial_server.o printf.o libserialsharedringbuffer/shared_ringbuffer.o xmodem/crc16.o xmodem/xmodem.o $(LWIPFILES:.c=.o) $(NETWORK_COMPONENTS)/lwip.o $(NETWORK_COMPONENTS)/utilization_socket.o $(NANOPBDIR)/pmu_sample.pb.o $(NANOPBDIR)/pb_common.o $(NANOPBDIR)/pb_encode.o
DUMMY_PROG_OBJS := dummy_prog.o
DUMMY_PROG2_OBJS := dummy_prog2.o


ETH_OBJS := $(ETHERNET_DRIVER)/ethernet.o $(ETH_RING_BUFFER)/shared_ringbuffer.o
ETH_MUX_RX_OBJS := $(NETWORK_COMPONENTS)/mux_rx.o $(ETH_RING_BUFFER)/shared_ringbuffer.o
ETH_MUX_TX_OBJS := $(NETWORK_COMPONENTS)/mux_tx_bandwidth_limited.o $(ETH_RING_BUFFER)/shared_ringbuffer.o
ETH_COPY_OBJS := $(NETWORK_COMPONENTS)/copy.o $(ETH_RING_BUFFER)/shared_ringbuffer.o
ARP_OBJS := cache.o $(LWIP)/core/inet_chksum.o $(LWIP)/core/def.o $(NETWORK_COMPONENTS)/arp.o $(ETH_RING_BUFFER)/shared_ringbuffer.o
TIMER_OBJS := $(TIMER_DRIVER)/timer.o

OBJS := $(sort $(addprefix $(BUILD_DIR)/, $(ETH_OBJS) $(ETH_MUX_RX_OBJS) $(ETH_MUX_TX_OBJS)\
	$(ETH_COPY_OBJS) \
	$(LWIP_OBJS) $(ARP_OBJS) $(TIMER_OBJS)))
DEPS := $(OBJS:.o=.d)


all: directories $(IMAGE_FILE)
-include $(DEPS)

$(BUILD_DIR)/%.d $(BUILD_DIR)/%.o: %.c Makefile
	mkdir -p `dirname $(BUILD_DIR)/$*.o`
	$(CC) -c $(CFLAGS) $< -o $(BUILD_DIR)/$*.o

$(BUILD_DIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile
	$(AS) -g3 -mcpu=$(CPU) $< -o $@

$(BUILD_DIR)/profiler.elf: $(addprefix $(BUILD_DIR)/, $(PROFILER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart.elf: $(addprefix $(BUILD_DIR)/, $(UART_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart_mux_rx.elf: $(addprefix $(BUILD_DIR)/, $(UART_MUX_RX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart_mux_tx.elf: $(addprefix $(BUILD_DIR)/, $(UART_MUX_TX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog2.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG2_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/client.elf: $(addprefix $(BUILD_DIR)/, $(CLIENT_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@
	
$(BUILD_DIR)/eth.elf: $(addprefix $(BUILD_DIR)/, $(ETH_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# $(BUILD_DIR)/lwip.elf: $(addprefix $(BUILD_DIR)/, $(LWIP_OBJS))
# 	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/eth_mux_rx.elf: $(addprefix $(BUILD_DIR)/, $(ETH_MUX_RX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/eth_mux_tx.elf: $(addprefix $(BUILD_DIR)/, $(ETH_MUX_TX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/eth_copy.elf: $(addprefix $(BUILD_DIR)/, $(ETH_COPY_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/arp.elf: $(addprefix $(BUILD_DIR)/, $(ARP_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/timer.elf: $(addprefix $(BUILD_DIR)/, $(TIMER_OBJS))
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
	$(info $(shell mkdir -p $(BUILD_DIR)/uart))	\

clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm


	