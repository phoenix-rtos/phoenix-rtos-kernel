#
# Makefile for Phoenix-RTOS kernel (RISCV64 HAL)
#
# Copyright 2018, 2020, 2024 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/, _init.o _cache.o _string.o _interrupts.o hal.o spinlock.o interrupts.o \
	cpu.o pmap.o dtb.o timer.o string.o exceptions.o plic.o sbi.o)

include hal/$(TARGET_SUFF)/$(TARGET_SUBFAMILY)/Makefile

CFLAGS += -Ihal/$(TARGET_SUFF) -Ihal/$(TARGET_SUFF)/$(TARGET_SUBFAMILY)

