#
# Makefile for Phoenix-RTOS kernel (ARMv7-M HAL)
#
# Copyright 2016-2017, 2019-2020 Phoenix Systems
#


ifneq (, $(findstring stm32, $(TARGET_SUBFAMILY)))
    include hal/armv7m/stm32/Makefile
    CFLAGS += -Ihal/armv7m/stm32
else ifneq (, $(findstring imxrt, $(TARGET_SUBFAMILY)))
    include hal/armv7m/imxrt/Makefile
    CFLAGS += -Ihal/armv7m/imxrt
endif

include hal/arm/Makefile
CFLAGS += -Ihal/armv7m -Ihal/arm

OBJS += $(addprefix $(PREFIX_O)hal/arm/, spinlock.o)
OBJS += $(addprefix $(PREFIX_O)hal/armv7m/, string.o cpu.o hal.o pmap.o exceptions.o _memcpy.o _memset.o)
