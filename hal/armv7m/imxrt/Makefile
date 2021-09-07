#
# Makefile for Phoenix-RTOS kernel (ARMv7 HAL)
#
# Copyright 2016-2017, 2019-2020 Phoenix Systems
#


ifneq (, $(findstring imxrt117x, $(TARGET_SUBFAMILY)))
    include hal/armv7m/imxrt/117x/Makefile
else ifneq (, $(findstring imxrt106x, $(TARGET_SUBFAMILY)))
    include hal/armv7m/imxrt/10xx/Makefile
else ifneq (, $(findstring imxrt105x, $(TARGET_SUBFAMILY)))
    include hal/armv7m/imxrt/10xx/Makefile
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv7m/imxrt/, _init.o interrupts.o pmap.o timer.o)
