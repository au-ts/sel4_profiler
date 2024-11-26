MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit
BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)
SYSTEM_FILE := ${ROOTDIR}/example/board/$(MICROKIT_BOARD)/profiler.system
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
TIMER_DRIVER:=$(SDDF)/drivers/timer/$(TIMER_DRV_DIR)
NETWORK_COMPONENTS:=$(SDDF)/network/components
PROF_EXAMPLE:=${ROOTDIR}/example

vpath %.c ${PROF_EXAMPLE} ${ROOT_DIR}

IMAGES :=   prof_client.elf profiler.elf eth_driver.elf network_virt_rx.elf \
			network_virt_tx.elf copy.elf timer_driver.elf uart_driver.elf serial_virt_tx.elf \
			serial_virt_rx.elf dummy_prog.elf dummy_prog2.elf

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
	  -MD \
	  -MP

LDFLAGS := -L$(BOARD_DIR)/lib -L${LIBC}
LIBS := --start-group -lmicrokit -Tmicrokit.ld -lc libsddf_util_debug.a --end-group

%.elf: %.o
	$(LD) $(LDFLAGS) $< $(LIBS) -o $@

# Build the test system
DUMMY_PROG_OBJS := dummy_prog.o
DUMMY_PROG2_OBJS := dummy_prog2.o

OBJS := $(DUMMY_PROG_OBJS) $(DUMMY_PROG2_OBJS)
DEPS := $(filter %.d,$(OBJS:.o=.d))

dummy_prog.elf: $(DUMMY_PROG_OBJS) libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

dummy_prog2.elf: $(DUMMY_PROG2_OBJS) libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# This shouldn't be needed as make *should* use the default CC recipe.
%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@


# Need to build libsddf_util_debug.a because it's included in LIBS
# for the unimplemented libc dependencies
${IMAGES}: libsddf_util.a libsddf_util_debug.a

${IMAGE_FILE} $(REPORT_FILE): $(IMAGES) $(SYSTEM_FILE)
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

include ${SDDF}/util/util.mk
include $(ROOTDIR)/profiler_client/profiler_client.mk
include $(ROOTDIR)/profiler/profiler.mk
include ${SDDF}/network/components/network_components.mk
include ${ETHERNET_DRIVER}/eth_driver.mk
include ${TIMER_DRIVER}/timer_driver.mk
include ${UART_DRIVER}/uart_driver.mk
include ${SERIAL_COMPONENTS}/serial_components.mk

-include $(DEPS)
