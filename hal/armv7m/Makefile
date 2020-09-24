#
# Makefile for Phoenix-RTOS kernel (ARMv7 HAL)
#
# Copyright 2016-2017, 2019-2020 Phoenix Systems
#


ifneq (, $(findstring stm32, $(TARGET_SUBFAMILY)))
    include hal/armv7m/stm32/Makefile
    CFLAGS += -Ihal/armv7m/stm32 -Ihal/armv7m
else ifneq (, $(findstring imxrt, $(TARGET_SUBFAMILY)))
    include hal/armv7m/imxrt/Makefile 
    CFLAGS += -Ihal/armv7m/imxrt -Ihal/armv7m
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv7m/, spinlock.o cpu.o hal.o exceptions.o)
