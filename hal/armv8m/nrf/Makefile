#
# Makefile for Phoenix-RTOS kernel (ARMv8 HAL)
#
# Copyright 2016-2017, 2019-2020, 2022 Phoenix Systems
#


ifneq (, $(findstring nrf91, $(TARGET_SUBFAMILY)))
    include hal/armv8m/nrf/91/Makefile
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv8m/nrf/, _init.o)
