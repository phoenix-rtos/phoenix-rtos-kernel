#
# Makefile for Phoenix-RTOS kernel (ARMv7-A HAL)
#
# Copyright 2018, 2020, 2021 Phoenix Systems
#

ifneq (, $(findstring zynq7000, $(TARGET_SUBFAMILY)))
    include hal/armv7a/zynq7000/Makefile
    CFLAGS += -Ihal/armv7a/zynq7000
else ifneq (, $(findstring imx6ull, $(TARGET_SUBFAMILY)))
    include hal/armv7a/imx6ull/Makefile
    CFLAGS += -Ihal/armv7a/imx6ull
endif

CFLAGS += -Ihal/armv7a -Ihal/arm -DKERNEL_SPINLOCK_ARM_M=0

OBJS += $(addprefix $(PREFIX_O)hal/arm/, spinlock.o)
OBJS += $(addprefix $(PREFIX_O)hal/armv7a/, string.o cpu.o hal.o pmap.o exceptions.o _memcpy.o _memset.o)
