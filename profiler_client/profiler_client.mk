#
# Copyright 2022, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#

# The issue with this system is that we are NOT executing this build snippet in the
# build directory. We need to therefore have a different approach.

include ${SDDF}/$(LWIPDIR)/Filelists.mk

# NETIFFILES: Files implementing various generic network interface functions
# Override version in Filelists.mk
NETIFFILES:=$(LWIPDIR)/netif/ethernet.c

# LWIPFILES: All the above.
LWIPFILES=  $(addprefix $(SDDF)/, $(COREFILES) $(CORE4FILES) $(NETIFFILES))
# Do we need to add timer client into here?
LWIP_OBJS :=  $(LWIPFILES:.c=.o) $(ROOTDIR)/profiler_client/lwip.o $(ROOTDIR)/profiler_client/netconn_socket.o $(ROOTDIR)/profiler_client/client.o \
			$(PROTOBUFDIR)/nanopb/pmu_sample.pb.o $(PROTOBUFDIR)/nanopb/pb_common.o $(PROTOBUFDIR)/nanopb/pb_encode.o

# OBJS := $(addprefix $(BUILD_DIR)/, $(LWIP_OBJS))
OBJS:= $(LWIP_OBJS)
LWIP_DEPS = $(OBJS:.o=.d)

-include $(LWIP_DEPS)

%.d %.o: %.c Makefile| prof_client
	$(CC) -c $(CFLAGS) $< -o prof_client/$(notdir $*).o

%.o: %.s Makefile| prof_client
	$(AS) -g3 -mcpu=$(CPU) $< -o $@

prof_client: all
	mkdir -p $@

prof_client.elf: $(addprefix prof_client/, $(notdir $(LWIP_OBJS)))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

%.o:
	$(CC) $(CFLAGS) -c $(@:.o=.c) -o $@

all: prof_client


.PHONY: all depend compile clean

