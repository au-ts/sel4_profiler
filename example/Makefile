#
# Copyright 2022, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#


BUILD_DIR ?= build
MICROKIT_CONFIG ?= profiler

ifeq ($(strip $(MICROKIT_SDK)),)
$(error MICROKIT_SDK must be specified)
endif

ifeq ($(strip $(MICROKIT_BOARD)),)
$(error MICROKIT_BOARD must be specified)
endif

ifeq ($(MICROKIT_BOARD),maaxboard)
SDDF_PLATFORM_DIR := imx
CPU := cortex-a55
else ifeq ($(MICROKIT_BOARD),odroidc4)
SDDF_PLATFORM_DIR := meson
CPU := cortex-a53
endif

SYSTEM_DESC := board/$(MICROKIT_BOARD)/profiler.system

TOOLCHAIN := aarch64-none-elf

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit

ROOTDIR=$(abspath ..)
SDDF=$(ROOTDIR)/sDDF
SDDF_INCLUDE=$(SDDF)/include/sddf
LWIP=$(SDDF)/network/ipstacks/lwip/src
NETWORK_RING_BUFFER=$(SDDF)/network/libethsharedringbuffer
SDDF_NETWORK_COMPONENTS=$(SDDF)/network/components
ETHERNET_DRIVER=$(SDDF)/drivers/network/$(SDDF_PLATFORM_DIR)
TIMER_DRIVER=$(SDDF)/drivers/clock/$(SDDF_PLATFORM_DIR)
UTIL=$(SDDF)/util
UART_COMPONENTS=$(SDDF)/serial/components
UART_DRIVER=$(SDDF)/drivers/serial/$(SDDF_PLATFORM_DIR)

PROTOBUFDIR=$(ROOTDIR)/protobuf
ECHODIR=echo_server
PROFDIR=$(ROOTDIR)/profiler
SERIALDIR=$(ROOTDIR)/serial
BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)

IMAGES := profiler.elf client.elf uart.elf uart_virt_rx.elf uart_virt_tx.elf eth.elf eth_virt_rx.elf eth_virt_tx.elf eth_copy.elf arp.elf timer.elf echo.elf dummy_prog.elf dummy_prog2.elf
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 -Wall  -Wno-unused-function -fno-omit-frame-pointer
LDFLAGS := -L$(BOARD_DIR)/lib -L$(SDDF)/lib
LIBS := -lmicrokit -Tmicrokit.ld -lc

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

CFLAGS += -I$(BOARD_DIR)/include \
	-I$(ROOTDIR)/include	\
	-I$(SDDF)/include \
	-I$(BOARD_DIR)/include/sys \
	-I$(UART_DRIVER)/include \
	-I$(LWIP)/include \
	-I$(LWIP)/include/ipv4 \
	-I$(PROTOBUFDIR)/nanopb \
	-I$(ROOTDIR)/include/ethernet_config \
	-DSERIAL_NUM_CLIENTS=1 \
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
PROF_LWIPFILES=$(PROFDIR)/lwip.c $(COREFILES) $(CORE4FILES) $(NETIFFILES)
ECHO_LWIPFILES=$(ECHODIR)/lwip.c $(COREFILES) $(CORE4FILES) $(NETIFFILES)

UART_OBJS := $(UART_DRIVER)/uart.o printf.o putchar_debug.o
UART_VIRT_TX_OBJS := $(UART_COMPONENTS)/virt_tx.o
UART_VIRT_RX_OBJS := $(UART_COMPONENTS)/virt_rx.o
PROFILER_OBJS := $(PROFDIR)/profiler.o sddf_network_sharedringbuffer.o printf.o putchar_debug.o
CLIENT_OBJS := $(PROFDIR)/client.o sddf_network_sharedringbuffer.o  \
 $(PROF_LWIPFILES:.c=.o) $(PROFDIR)/lwip.o $(PROFDIR)/netconn_socket.o sddf_timer_client.o \
 $(PROTOBUFDIR)/nanopb/pmu_sample.pb.o $(PROTOBUFDIR)/nanopb/pb_common.o $(PROTOBUFDIR)/nanopb/pb_encode.o \
 $(SERIALDIR)/profiler_printf.o printf.o putchar_debug.o

ECHO_OBJS := $(ECHO_LWIPFILES:.c=.o) $(ECHODIR)/lwip.o sddf_network_sharedringbuffer.o  \
$(ECHODIR)/udp_echo_socket.o $(ECHODIR)/tcp_echo_socket.o printf.o putchar_serial.o \
sddf_timer_client.o
DUMMY_PROG_OBJS := dummy_prog.o
DUMMY_PROG2_OBJS := dummy_prog2.o

ETH_OBJS := $(ETHERNET_DRIVER)/ethernet.o sddf_network_sharedringbuffer.o printf.o putchar_debug.o
ETH_VIRT_RX_OBJS := $(SDDF_NETWORK_COMPONENTS)/mux_rx.o sddf_network_sharedringbuffer.o printf.o putchar_debug.o
ETH_VIRT_TX_OBJS := $(SDDF_NETWORK_COMPONENTS)/mux_tx.o sddf_network_sharedringbuffer.o $(UTIL)/cache.o printf.o putchar_debug.o
ETH_COPY_OBJS := $(SDDF_NETWORK_COMPONENTS)/copy.o sddf_network_sharedringbuffer.o printf.o putchar_debug.o
ARP_OBJS := $(UTIL)/cache.o $(LWIP)/core/inet_chksum.o $(LWIP)/core/def.o $(SDDF_NETWORK_COMPONENTS)/arp.o sddf_network_sharedringbuffer.o printf.o putchar_debug.o
TIMER_OBJS := $(TIMER_DRIVER)/timer.o printf.o putchar_debug.o

OBJS := $(sort $(addprefix $(BUILD_DIR)/, $(ETH_OBJS) $(ETH_VIRT_RX_OBJS) $(ETH_VIRT_TX_OBJS)\
	$(ETH_COPY_OBJS) $(CLIENT_OBJS)\
	$(ARP_OBJS) $(TIMER_OBJS) $(ECHO_OBJS)))
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

$(BUILD_DIR)/sddf_network_sharedringbuffer.o:
	BUILD_DIR=$(abspath $(BUILD_DIR)) MICROKIT_INCLUDE=$(BOARD_DIR)/include make -C $(NETWORK_RING_BUFFER)

$(BUILD_DIR)/sddf_timer_client.o:
	BUILD_DIR=$(abspath $(BUILD_DIR)) MICROKIT_INCLUDE=$(BOARD_DIR)/include TIMER_CHANNEL=9 make -C $(SDDF)/timer/client

$(BUILD_DIR)/printf.o: $(UTIL)/printf.c $(SDDF_INCLUDE)/util/printf.h
	$(CC) -c $(CFLAGS) $(UTIL)/printf.c -o $(BUILD_DIR)/printf.o

$(BUILD_DIR)/putchar_debug.o: $(UTIL)/putchar_debug.c
	$(CC) -c $(CFLAGS) $(UTIL)/putchar_debug.c -o $(BUILD_DIR)/putchar_debug.o

$(BUILD_DIR)/putchar_serial.o: $(UTIL)/putchar_serial.c
	$(CC) -c $(CFLAGS) $(UTIL)/putchar_serial.c -o $(BUILD_DIR)/putchar_serial.o

$(BUILD_DIR)/profiler.elf: $(addprefix $(BUILD_DIR)/, $(PROFILER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart.elf: $(addprefix $(BUILD_DIR)/, $(UART_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart_virt_rx.elf: $(addprefix $(BUILD_DIR)/, $(UART_VIRT_RX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart_virt_tx.elf: $(addprefix $(BUILD_DIR)/, $(UART_VIRT_TX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/client.elf: $(addprefix $(BUILD_DIR)/, $(CLIENT_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/eth.elf: $(addprefix $(BUILD_DIR)/, $(ETH_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/eth_virt_rx.elf: $(addprefix $(BUILD_DIR)/, $(ETH_VIRT_RX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/eth_virt_tx.elf: $(addprefix $(BUILD_DIR)/, $(ETH_VIRT_TX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/eth_copy.elf: $(addprefix $(BUILD_DIR)/, $(ETH_COPY_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/arp.elf: $(addprefix $(BUILD_DIR)/, $(ARP_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/timer.elf: $(addprefix $(BUILD_DIR)/, $(TIMER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/echo.elf: $(addprefix $(BUILD_DIR)/, $(ECHO_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/dummy_prog2.elf: $(addprefix $(BUILD_DIR)/, $(DUMMY_PROG2_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) $(SYSTEM_DESC)
	$(MICROKIT_TOOL) $(SYSTEM_DESC) --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

.PHONY: all depend compile clean

%.o:
	$(CC) $(CFLAGS) -c $(@:.o=.c) -o $@

#Make the Directories
directories:
	$(info $(shell mkdir -p $(BUILD_DIR)/uart))	\

clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm


	
