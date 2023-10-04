#
# Makefile for Phoenix-RTOS kernel (ARMv7-M HAL STM32)
#
# Copyright 2016-2017, 2019-2020 Phoenix Systems
#


ifneq (, $(findstring l1, $(TARGET_SUBFAMILY)))
    include hal/armv7m/stm32/l1/Makefile
else ifneq (, $(findstring l4, $(TARGET_SUBFAMILY)))
    include hal/armv7m/stm32/l4/Makefile
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv7m/stm32/, _init.o interrupts.o)
