# This is a test makefile for the profiler system
export MICROKIT_CONFIG ?= profiler
export BUILD_DIR:=$(abspath build)
# export BUILD_DIR := build

ifeq ($(strip $(TOOLCHAIN)),)
	TOOLCHAIN := aarch64-none-elf
	export TOOLCHAIN := aarch64-none-elf
	export LIBC := $(dir $(realpath $(shell aarch64-none-elf-gcc --print-file-name libc.a)))
endif

export CC := $(TOOLCHAIN)-gcc
export LD := $(TOOLCHAIN)-ld
export AS := $(TOOLCHAIN)-as
export AR := $(TOOLCHAIN)-ar
export RANLIB := $(TOOLCHAIN)-ranlib
export SDDF=$(abspath ../sDDF)
export ROOTDIR=$(abspath ../)
export MICROKIT_SDK:=$(abspath ${MICROKIT_SDK})
# Just for test CPU
export CPU := cortex-a55
IMAGE_FILE := $(BUILD_DIR)/loader.img
REPORT_FILE := $(BUILD_DIR)/report.txt


all: ${IMAGE_FILE}

qemu ${IMAGE_FILE} ${REPORT_FILE} clean clobber: ${BUILD_DIR}/Makefile FORCE
	${MAKE} -C ${BUILD_DIR}  MICROKIT_SDK=${MICROKIT_SDK} $(notdir $@)

${BUILD_DIR}/Makefile: test.mk
	mkdir -p ${BUILD_DIR}
	cp test.mk $@

FORCE:
