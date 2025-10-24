#
# Makefile for Phoenix-RTOS kernel (ARMv7-M HAL)
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


ifdef MPUTEST_ASMOPT
    CFLAGS += -DMPUTEST_ASMOPT
endif

ifdef MPUTEST_FORCEALL
    CFLAGS += -DMPUTEST_FORCEALL
endif

ifdef MPUTEST_ORGIMPL
    CFLAGS += -DMPUTEST_ORGIMPL
endif

ifdef MPUTEST_ORGIMPL_OPT
    CFLAGS += -DMPUTEST_ORGIMPL
    CFLAGS += -DMPUTEST_ORGIMPL_OPT
endif

ifdef MPUTEST_FIXED
    CFLAGS += -DMPUTEST_FIXED
endif



include hal/arm/Makefile
CFLAGS += -Ihal/arm

OBJS += $(addprefix $(PREFIX_O)hal/armv7m/, string.o spinlock.o cpu.o hal.o pmap.o exceptions.o _memcpy.o _memset.o)
