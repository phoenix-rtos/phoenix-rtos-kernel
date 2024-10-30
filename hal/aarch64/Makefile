#
# Makefile for Phoenix-RTOS kernel (ARMv8-A HAL)
#
# Copyright 2018, 2020, 2021, 2024 Phoenix Systems
#

CFLAGS += -Ihal/aarch64
ifneq (, $(findstring zynqmp, $(TARGET_SUBFAMILY)))
    include hal/aarch64/zynqmp/Makefile
    CFLAGS += -Ihal/aarch64/zynqmp
    OBJS += $(addprefix $(PREFIX_O)hal/aarch64/, interrupts.o)
endif

OBJS += $(addprefix $(PREFIX_O)hal/aarch64/, cache.o cpu.o dtb.o hal.o exceptions.o pmap.o spinlock.o string.o)
