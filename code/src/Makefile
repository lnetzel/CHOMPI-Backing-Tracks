# CHOMPI-BACKING-TRACKS

TARGET = CHOMPI_BACKING_TRACKS_1.0

DEBUG=0
OPT=-O3

APP_TYPE=BOOT_SRAM
# `make program-boot` installs CHOMPI's own bootloader (bin/ in this repo)
BOOT_BIN = ../../bin/CHOMPI_Bootloader_V6_2_0.bin
LDSCRIPT = chompi_sram.lds

# Sources
CPP_SOURCES = backing_tracks_main.cpp \
	encoder.cpp

# SD-card presets: FatFS + coreJSON (vendored in code/libs)
USE_FATFS = 1
C_SOURCES = ../libs/coreJSON/source/core_json.c
C_INCLUDES += -I../libs/coreJSON/source/include

# Platform support: CHOMPI-adapted libDaisy in this repo (prebuilt .a committed)
LIBDAISY_DIR = ../libs/libDaisy

# Optional audio-CPU meter on PTH LED 0: build with `make BACKING_CPU_METER=1`
ifeq ($(BACKING_CPU_METER),1)
C_DEFS += -DBACKING_CPU_METER
endif

# Core location, and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

.PHONY: check-toolchain
check-toolchain:
	@command -v arm-none-eabi-gcc >/dev/null || { echo "arm-none-eabi-gcc is missing from PATH"; exit 1; }
	@command -v arm-none-eabi-g++ >/dev/null || { echo "arm-none-eabi-g++ is missing from PATH"; exit 1; }
	@command -v make >/dev/null || { echo "make is missing from PATH"; exit 1; }
	@arm-none-eabi-gcc --version | head -n 1
	@arm-none-eabi-gcc --version | grep -q '10\.3\.1' || { echo "This firmware requires Arm GCC 10.3.1"; exit 1; }
