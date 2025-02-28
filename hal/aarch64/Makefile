#
# Makefile for Phoenix-RTOS kernel (AArch64 HAL)
#
# Copyright 2018, 2020, 2021, 2024 Phoenix Systems
#

# Note: _init.o must be first in the list
OBJS += $(addprefix $(PREFIX_O)hal/aarch64/, _init.o cache.o cpu.o dtb.o hal.o exceptions.o pmap.o spinlock.o string.o _memcpy.o _memset.o)

CFLAGS += -mcpu=$(cpu)+nofp -Ihal/aarch64
ifneq (, $(findstring zynqmp, $(TARGET_SUBFAMILY)))
    include hal/aarch64/zynqmp/Makefile
    CFLAGS += -Ihal/aarch64/zynqmp
    OBJS += $(addprefix $(PREFIX_O)hal/aarch64/, interrupts_gicv2.o)
endif

$(PREFIX_O)hal/aarch64/_init.o: hal/aarch64/_exceptions.S hal/aarch64/_init.S
