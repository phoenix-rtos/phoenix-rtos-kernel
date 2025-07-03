#
# Makefile for Phoenix-RTOS kernel (ARMv8-M HAL STM32)
#
# Copyright 2016-2025 Phoenix Systems
#


ifneq (, $(findstring n6, $(TARGET_SUBFAMILY)))
    include hal/armv8m/stm32/n6/Makefile
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv8m/stm32/, _init.o)
