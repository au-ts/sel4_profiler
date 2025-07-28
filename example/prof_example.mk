MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit
BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)
SYSTEM_FILE := profiler.system
IMAGE_FILE := loader.img
REPORT_FILE := report.txt
LWIPDIR:=network/ipstacks/lwip/src
PROTOBUFDIR := ${ROOTDIR}/protobuf
UTIL:=$(SDDF)/util
ETHERNET_CONFIG_INCLUDE:=$(PROFILER)/include/ethernet_config
SERIAL_CONFIG_INCLUDE:=$(PROFILER)/include/serial_config
SERIAL_COMPONENTS := $(SDDF)/serial/components
UART_DRIVER := $(SDDF)/drivers/serial/$(UART_DRIV_DIR)
ETHERNET_DRIVER:=$(SDDF)/drivers/network/$(DRIV_DIR)
NETWORK_COMPONENTS:=$(SDDF)/network/components
SDDF_LWIP:=$(SDDF)/network/lib_sddf_lwip
TIMER_DRIVER:=$(SDDF)/drivers/timer/$(TIMER_DRV_DIR)
NETWORK_COMPONENTS:=$(SDDF)/network/components
PROF_EXAMPLE:=${ROOTDIR}/example
ECHO_DIR:=${PROF_EXAMPLE}/echo_server
OBJCOPY:=$(TOOLCHAIN)-objcopy

PYTHON ?= python3
DTC := dtc
METAPROGRAM := $(PROF_EXAMPLE)/meta.py
DTS := $(SDDF)/dts/$(MICROKIT_BOARD).dts
DTB := $(MICROKIT_BOARD).dtb

SDFGEN_HELPER := $(PROF_EXAMPLE)/sdfgen_helper.py
PROFILER_CONFIG_HEADERS := $(SDDF)/include/sddf/resources/common.h \
							$(ROOTDIR)/include/config.h

vpath %.c ${PROF_EXAMPLE} ${ROOT_DIR} ${ECHO_DIR}

IMAGES :=   prof_client.elf profiler.elf eth_driver.elf network_virt_rx.elf \
			network_virt_tx.elf copy.elf timer_driver.elf serial_driver.elf serial_virt_tx.elf \
			serial_virt_rx.elf dummy_prog1.elf dummy_prog2.elf

CFLAGS := -mcpu=$(CPU) \
	  -mstrict-align \
	  -ffreestanding \
	  -g3 -O3 -Wall \
	  -Wno-unused-function \
	  -DMICROKIT_CONFIG_$(MICROKIT_CONFIG) \
	  -I$(BOARD_DIR)/include \
	  -I$(SDDF)/include \
	  -I${ECHO_INCLUDE}/lwip \
	  -I${ETHERNET_CONFIG_INCLUDE} \
	  -I$(SERIAL_CONFIG_INCLUDE) \
	  -I${SDDF}/$(LWIPDIR)/include \
	  -I${SDDF}/$(LWIPDIR)/include/ipv4 \
	  -I${ROOTDIR}/include \
	  -I$(SEL4_SDK)/include \
	  -I$(PROTOBUFDIR)/nanopb \
	  -I$(SDDF)/include/microkit \
	  -MD \
	  -MP

LDFLAGS := -L$(BOARD_DIR)/lib -L${LIBC}
LIBS := --start-group -lmicrokit -Tmicrokit.ld -lc libsddf_util_debug.a --end-group

%.elf: %.o
	$(LD) $(LDFLAGS) $< $(LIBS) -o $@

# Build the test system
include ${SDDF}/${LWIPDIR}/Filelists.mk

# NETIFFILES: Files implementing various generic network interface functions
# Override version in Filelists.mk
NETIFFILES:=$(LWIPDIR)/netif/ethernet.c

# LWIPFILES: All the above.
LWIPFILES=lwip.c $(COREFILES) $(CORE4FILES) $(NETIFFILES)
ECHO_OBJS := $(LWIPFILES:.c=.o) lwip.o \
	     udp_echo_socket.o tcp_echo_socket.o

DUMMY_PROG_OBJS := dummy_prog.o
DUMMY_PROG2_OBJS := dummy_prog2.o

OBJS := $(DUMMY_PROG_OBJS) $(DUMMY_PROG2_OBJS) $(ECHO_OBJS)
DEPS := $(filter %.d,$(OBJS:.o=.d))

dummy_prog.elf: $(DUMMY_PROG_OBJS) libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

dummy_prog2.elf: $(DUMMY_PROG2_OBJS) libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

${ECHO_OBJS}: ${CHECK_FLAGS_BOARD_MD5}
echo.elf: $(ECHO_OBJS) libsddf_util.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# Need to build libsddf_util_debug.a because it's included in LIBS
# for the unimplemented libc dependencies
${IMAGES}: libsddf_util.a libsddf_util_debug.a

$(DTB): $(DTS)
	dtc -q -I dts -O dtb $(DTS) > $(DTB)

$(SYSTEM_FILE): $(METAPROGRAM) $(IMAGES) $(DTB)
	$(PYTHON) $(SDFGEN_HELPER) --configs "$(PROFILER_CONFIG_HEADERS)" --output $(PROF_EXAMPLE)/config_structs.py
	$(PYTHON) $(METAPROGRAM) --sddf $(SDDF) --board $(MICROKIT_BOARD) --dtb $(DTB) --output . --sdf $(SYSTEM_FILE)
# Profiler config
	$(OBJCOPY) --update-section .profiler_config=profiler_conn.data profiler.elf
	$(OBJCOPY) --update-section .profiler_config=prof_client_conn.data prof_client.elf
# Device class configs
	$(OBJCOPY) --update-section .device_resources=uart_driver_device_resources.data serial_driver.elf
	$(OBJCOPY) --update-section .serial_driver_config=serial_driver_config.data serial_driver.elf
	$(OBJCOPY) --update-section .serial_virt_tx_config=serial_virt_tx.data serial_virt_tx.elf
	$(OBJCOPY) --update-section .device_resources=ethernet_driver_device_resources.data eth_driver.elf
	$(OBJCOPY) --update-section .net_driver_config=net_driver.data eth_driver.elf
	$(OBJCOPY) --update-section .net_virt_rx_config=net_virt_rx.data network_virt_rx.elf
	$(OBJCOPY) --update-section .net_virt_tx_config=net_virt_tx.data network_virt_tx.elf
	$(OBJCOPY) --update-section .device_resources=timer_driver_device_resources.data timer_driver.elf
# Client configs
# 	$(OBJCOPY) --update-section .timer_client_config=timer_client_echo.data echo.elf
# 	$(OBJCOPY) --update-section .net_client_config=net_client_echo.data echo.elf
# 	$(OBJCOPY) --update-section .serial_client_config=serial_client_echo.data echo.elf
	$(OBJCOPY) --update-section .timer_client_config=timer_client_prof_client.data prof_client.elf
	$(OBJCOPY) --update-section .net_client_config=net_client_prof_client.data prof_client.elf
	$(OBJCOPY) --update-section .serial_client_config=serial_client_prof_client.data prof_client.elf
# Lwip configs
# 	$(OBJCOPY) --update-section .lib_sddf_lwip_config=lib_sddf_lwip_config_echo.data echo.elf
	$(OBJCOPY) --update-section .lib_sddf_lwip_config=lib_sddf_lwip_config_prof_client.data prof_client.elf

${IMAGE_FILE} $(REPORT_FILE): $(IMAGES) $(SYSTEM_FILE)
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

include ${SDDF}/util/util.mk
include $(ROOTDIR)/profiler_client/profiler_client.mk
include $(ROOTDIR)/profiler/profiler.mk
include ${NETWORK_COMPONENTS}/network_components.mk
include ${ETHERNET_DRIVER}/eth_driver.mk
include ${SDDF_LWIP}/lib_sddf_lwip.mk
include ${TIMER_DRIVER}/timer_driver.mk
include ${UART_DRIVER}/serial_driver.mk
include ${SERIAL_COMPONENTS}/serial_components.mk

-include $(DEPS)
