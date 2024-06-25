#
# Makefile for Phoenix-RTOS kernel (ARMv8 HAL)
#
# Copyright 2016-2017, 2019-2020, 2022, 2024 Phoenix Systems
#


ifneq (, $(findstring mcxn94x, $(TARGET_SUBFAMILY)))
    include hal/armv8m/mcx/n94x/Makefile
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv8m/mcx/, _init.o interrupts.o)
