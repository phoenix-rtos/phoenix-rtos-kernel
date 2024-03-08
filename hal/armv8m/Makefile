#
# Makefile for Phoenix-RTOS kernel (ARMv8M HAL)
#
# Copyright 2022 Phoenix Systems
#


ifneq (, $(findstring nrf, $(TARGET_SUBFAMILY)))
    include hal/armv8m/nrf/Makefile
    CFLAGS += -Ihal/armv8m
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv8m/, string.o spinlock.o cpu.o hal.o pmap.o exceptions.o _memcpy.o _memset.o)
