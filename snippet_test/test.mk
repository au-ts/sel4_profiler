MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit
BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)
SYSTEM_FILE := ${ROOTDIR}/example/board/$(MICROKIT_BOARD)/profiler.system
IMAGE_FILE := loader.img
REPORT_FILE := report.txt
IMAGES := prof_client.elf
LWIPDIR:=network/ipstacks/lwip/src
PROTOBUFDIR := ${ROOTDIR}/protobuf
UTIL:=$(SDDF)/util
ETHERNET_CONFIG_INCLUDE:=$(PROFILER)/include/ethernet_config
SERIAL_CONFIG_INCLUDE:=$(PROFILER)/include/serial_config

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

# Need to build libsddf_util_debug.a because it's included in LIBS
# for the unimplemented libc dependencies
${IMAGES}: libsddf_util_debug.a

${IMAGE_FILE} $(REPORT_FILE): $(IMAGES) $(SYSTEM_FILE)
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)


# include $(ROOTDIR)/profiler/profiler.mk
include $(ROOTDIR)/profiler_client/profiler_client.mk
include ${SDDF}/util/util.mk
