#
# Makefile for Phoenix-RTOS kernel (ARMv8-R HAL)
#
# Copyright 2024 Phoenix Systems
#

ifneq (, $(findstring mps3an536, $(TARGET_SUBFAMILY)))
  include hal/armv8r/mps3an536/Makefile
  CFLAGS += -Ihal/armv8r/mps3an536
else
  $(error Unsupported TARGET)
endif

CFLAGS += -Ihal/armv8r

OBJS += $(addprefix $(PREFIX_O)hal/armv8r/, _armv8r.o _interrupts.o _memcpy.o _memset.o \
  cpu.o exceptions.o hal.o pmap.o spinlock.o string.o)
