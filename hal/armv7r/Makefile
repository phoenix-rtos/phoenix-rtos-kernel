#
# Makefile for Phoenix-RTOS kernel (ARMv7-R HAL)
#
# Copyright 2025 Phoenix Systems
#

ifneq (, $(findstring zynqmp, $(TARGET_SUBFAMILY)))
  include hal/armv7r/zynqmp/Makefile
  CFLAGS += -Ihal/armv7r/zynqmp
else
  $(error Unsupported TARGET)
endif

CFLAGS += -Ihal/armv7r

OBJS += $(addprefix $(PREFIX_O)hal/armv7r/, _armv7r.o _interrupts.o _memcpy.o _memset.o \
  cpu.o exceptions.o hal.o pmap.o spinlock.o string.o)
