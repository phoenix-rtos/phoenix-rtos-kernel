#
# Makefile for Phoenix-RTOS kernel (RISCV64 HAL)
#
# Copyright 2018, 2020, 2024 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/, _init.o _cache.o _string.o _interrupts.o hal.o spinlock.o interrupts.o \
	cpu.o pmap.o dtb.o timer.o string.o exceptions.o plic.o sbi.o)

CFLAGS += -Ihal/$(TARGET_SUFF)

ifneq ($(filter $(TARGET_SUBFAMILY),grfpga gr765),)
  include hal/$(TARGET_SUFF)/gaisler/Makefile
  CFLAGS += -Ihal/$(TARGET_SUFF)/gaisler/$(TARGET_SUBFAMILY)
else
  include hal/$(TARGET_SUFF)/$(TARGET_SUBFAMILY)/Makefile
  CFLAGS += -Ihal/$(TARGET_SUFF)/$(TARGET_SUBFAMILY)
endif

# binutils 2.41 silently introduced gp relaxations which for some reason make kernel impossible to build
# TODO: investigate further
ifeq ($(shell expr $(LD_VERSION_MINOR) ">=" 41), 1)
LDFLAGS += $(LDFLAGS_PREFIX)--no-relax-gp
endif
