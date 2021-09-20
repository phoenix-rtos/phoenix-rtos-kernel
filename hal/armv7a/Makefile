#
# Makefile for Phoenix-RTOS kernel (ARM HAL)
#
# Copyright 2018, 2020, 2021 Phoenix Systems
#

ifneq (, $(findstring zynq7000, $(TARGET_SUBFAMILY)))
    include hal/armv7a/zynq7000/Makefile
    CFLAGS += -Ihal/armv7a/zynq7000 -Ihal/armv7a
else ifneq (, $(findstring imx6ull, $(TARGET_SUBFAMILY)))
    include hal/armv7a/imx6ull/Makefile
    CFLAGS += -Ihal/armv7a/imx6ull -Ihal/armv7a
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv7a/, spinlock.o cpu.o hal.o pmap.o exceptions.o)
