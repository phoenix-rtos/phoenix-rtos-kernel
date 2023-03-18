#
# Makefile for Phoenix-RTOS kernel (sparcv8leon3 HAL)
#
# Copyright 2022 Phoenix Systems
#

include hal/sparcv8leon3/$(TARGET_SUBFAMILY)/Makefile

CFLAGS += -Ihal/sparcv8leon3 -Ihal/sparcv8leon3/$(TARGET_SUBFAMILY)

OBJS += $(addprefix $(PREFIX_O)hal/sparcv8leon3/, cpu.o exceptions.o hal.o interrupts.o pmap.o spinlock.o string.o _init.o _interrupts.o _traps.o)
