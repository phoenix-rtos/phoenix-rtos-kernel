#
# Makefile for phoenix-rtos-kernel/hal/ia32
#
# Copyright 2017, 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

include hal/tlb/Makefile

OBJS += $(addprefix $(PREFIX_O)hal/ia32/, _init.o _exceptions.o _interrupts.o spinlock.o exceptions.o interrupts.o cpu.o pmap.o timer.o hal.o string.o pci.o init.o console.o perf.o)
CFLAGS += -Ihal/ia32

ifneq ($(findstring serial,$(CONSOLE)),)
  OBJS += $(PREFIX_O)hal/$(TARGET_SUFF)/console-serial.o
  CPPFLAGS += -DHAL_CONSOLE_SERIAL
endif

ifneq ($(findstring vga,$(CONSOLE)),)
  OBJS += $(PREFIX_O)hal/$(TARGET_SUFF)/console-vga.o
  CPPFLAGS += -DHAL_CONSOLE_VGA
endif
