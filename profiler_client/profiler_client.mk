#
# Copyright 2022, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#

vpath %.c ${ROOTDIR} ${SDDF}

# The issue with this system is that we are NOT executing this build snippet in the
# build directory. We need to therefore have a different approach.

include ${SDDF}/$(LWIPDIR)/Filelists.mk

# NETIFFILES: Files implementing various generic network interface functions
# Override version in Filelists.mk
NETIFFILES:=$(LWIPDIR)/netif/ethernet.c

# LWIPFILES: All the above.
LWIPFILES=   $(COREFILES) $(CORE4FILES) $(NETIFFILES)
# Do we need to add timer client into here?

LWIP_OBJS := $(LWIPFILES:.c=.o)
PROF_CLIENT_OBJS :=  $(addprefix profiler_client/, lwip.o netconn_socket.o client.o) \
			$(addprefix protobuf/nanopb/, pmu_sample.pb.o pb_common.o pb_encode.o)

# OBJS := $(addprefix $(BUILD_DIR)/, $(LWIP_OBJS))
OBJS:= $(LWIP_OBJS) $(PROF_CLIENT_OBJS)
LWIP_DEPS := $(filter %.d,$(OBJS:.o=.d))

PROF_CLIENT_LIBS := --start-group -lmicrokit -Tmicrokit.ld -lc libsddf_util.a --end-group

%.elf: %.o
	$(LD) $(LDFLAGS) $< $(PROF_CLIENT_LIBS) -o $@

prof_client.elf: $(LWIP_OBJS) $(PROF_CLIENT_OBJS) libsddf_util.a
	$(LD) $(LDFLAGS) $^ $(PROF_CLIENT_LIBS) -o $@

PROFCLIENTDIR := profiler_client protobuf/nanopb
$(PROF_CLIENT_OBJS): | ${PROFCLIENTDIR}
${PROFCLIENTDIR}:
	mkdir -p $@

LWIPDIRS := $(addprefix ${LWIPDIR}/, core/ipv4 netif api)
${LWIP_OBJS}: |${LWIPDIRS}
${LWIPDIRS}:
	mkdir -p $@

-include $(LWIP_DEPS)
