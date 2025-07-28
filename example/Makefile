export MICROKIT_CONFIG ?= debug
# export BUILD_DIR:=$(abspath build)
export BUILD_DIR := build
ifeq ($(strip $(MICROKIT_SDK)),)
$(error MICROKIT_SDK must be specified)
endif
export override MICROKIT_SDK:=$(abspath ${MICROKIT_SDK})


ifeq ($(strip $(TOOLCHAIN)),)
	TOOLCHAIN := aarch64-none-elf
	export TOOLCHAIN := aarch64-none-elf
	export LIBC := $(dir $(realpath $(shell aarch64-none-elf-gcc --print-file-name libc.a)))
endif

ifeq ($(strip $(MICROKIT_BOARD)), odroidc4)
	export DRIV_DIR := meson
	export UART_DRIV_DIR := meson
	export TIMER_DRV_DIR := meson
	export CPU := cortex-a55
else ifeq ($(strip $(MICROKIT_BOARD)),maaxboard)
	export DRIV_DIR := imx
	export UART_DRIV_DIR := imx
	export TIMER_DRV_DIR := imx
	export CPU := cortex-a53
else
$(error Unsupported MICROKIT_BOARD given)
endif

export CC := $(TOOLCHAIN)-gcc
export LD := $(TOOLCHAIN)-ld
export AS := $(TOOLCHAIN)-as
export AR := $(TOOLCHAIN)-ar
export RANLIB := $(TOOLCHAIN)-ranlib
export SDDF=$(abspath ../sDDF)
export ROOTDIR=$(abspath ../)
export MICROKIT_SDK:=$(abspath ${MICROKIT_SDK})
IMAGE_FILE := $(BUILD_DIR)/loader.img
REPORT_FILE := $(BUILD_DIR)/report.txt

all: ${IMAGE_FILE}

qemu ${IMAGE_FILE} ${REPORT_FILE} clean clobber: ${BUILD_DIR}/Makefile FORCE
	${MAKE}  -C ${BUILD_DIR} MICROKIT_SDK=${MICROKIT_SDK} $(notdir $@)

${BUILD_DIR}/Makefile: prof_example.mk
	mkdir -p ${BUILD_DIR}
	cp prof_example.mk $@

FORCE:
