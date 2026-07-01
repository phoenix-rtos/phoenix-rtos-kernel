#
# Makefile for Phoenix-RTOS kernel (ARMv8-M HAL STM32U3)
#
# Copyright 2016-2017, 2019-2025 Phoenix Systems
# Copyright 2026 Apator Metrix
#

CFLAGS += -Ihal/armv8m/stm32/u3

OBJS += $(addprefix $(PREFIX_O)hal/armv8m/, _init-m33.o)
OBJS += $(addprefix $(PREFIX_O)hal/armv8m/stm32/u3/, stm32u3.o timer.o)
