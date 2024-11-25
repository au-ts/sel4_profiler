PROFILER_DIR := $(abspath $(dir $(lastword ${MAKEFILE_LIST})))
# How should we do this proto buf directory
PROTOBUFDIR=$(ROOTDIR)/protobuf
LWIPDIR=$(SDDF)/network/ipstacks/lwip/src
PROFILER_IMAGES := profiler.elf client.elf

CFLAGS += -I$(BOARD_DIR)/include \
	-I$(ROOTDIR)/include	\
	-I$(SDDF)/include \
	-I$(BOARD_DIR)/include/sys \
	-I$(LWIPDIR)/include \
	-I$(LWIPDIR)/include/ipv4 \
	-I$(PROTOBUFDIR)/nanopb \
	-I$(ROOTDIR)/include/ethernet_config \
	-I$(ROOTDIR)/include/serial_config \
	-MD \
	-MP

include ${LWIPDIR}/Filelists.mk

# NETIFFILES: Files implementing various generic network interface functions
# Override version in Filelists.mk
NETIFFILES:=$(LWIPDIR)/netif/ethernet.c

# LWIPFILES: All the above.
PROF_LWIPFILES=$(PROFILER_DIR)/lwip.c $(COREFILES) $(CORE4FILES) $(NETIFFILES)

PROFILER_COMPONENTS:= $(addprefix profiler/, profiler.o)
# @kwinter: I don't think we need to prefix all the things here.
PROFCLIENT_COMPONENTS := $(PROF_LWIPFILES:.c=.o)  $(addprefix profiler/, client.o, netconn_socket.o, lwip.o) \
					 $(addprefix protobuf/, nanopb/pmu_sample.pb.o, nanopb/pb_common.o \
					   nanopb/pb_encode.o)
OBJS := $(PROFILER_COMPONENTS) $(PROFCLIENT_COMPONENTS)
DEPS := $(filter %.d,$(OBJS:.o=.d))

LWIPDIRS := $(addprefix ${LWIPDIR}/, core/ipv4 netif api)
${LWIP_OBJS}: |${BUILD_DIR}/${LWIPDIRS}
${BUILD_DIR}/${LWIPDIRS}:
	mkdir -p $@

profiler.elf: $(addprefix $(BUILD_DIR)/, $(PROFILER_COMPONENTS)) libsddf_util.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

prof_client.elf: $(addprefix $(BUILD_DIR)/, $(CLIENT_COMPONENTS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

-include $(DEPS)