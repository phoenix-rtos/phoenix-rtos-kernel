#
# Makefile for Phoenix-RTOS kernel (ARMv8-M HAL STM32)
#
# Copyright 2016-2025 Phoenix Systems
# Copyright 2026 Apator Metrix
#


ifneq (, $(findstring n6, $(TARGET_SUBFAMILY)))
    include hal/armv8m/stm32/n6/Makefile
else ifneq (, $(findstring u3, $(TARGET_SUBFAMILY)))
    include hal/armv8m/stm32/u3/Makefile
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv8m/stm32/, console.o)
