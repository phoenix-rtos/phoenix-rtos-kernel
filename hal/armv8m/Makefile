#
# Makefile for Phoenix-RTOS kernel (ARMv8M HAL)
#
# Copyright 2022 Phoenix Systems
#


ifneq (, $(findstring nrf, $(TARGET_SUBFAMILY)))
    include hal/armv8m/nrf/Makefile
    CFLAGS += -Ihal/armv8m
else ifneq (, $(findstring mcx, $(TARGET_SUBFAMILY)))
    include hal/armv8m/mcx/Makefile
    CFLAGS += -Ihal/armv8m
else ifneq (, $(findstring stm32, $(TARGET_SUBFAMILY)))
    include hal/armv8m/stm32/Makefile
    CFLAGS += -Ihal/armv8m
endif

include hal/arm/Makefile
CFLAGS += -Ihal/arm


ifdef MPUTEST_ASMOPT
    CFLAGS += -DMPUTEST_ASMOPT
endif

ifdef MPUTEST_ORGIMPL
    CFLAGS += -DMPUTEST_ORGIMPL
endif

ifdef MPUTEST_ORGIMPL_OPT
    CFLAGS += -DMPUTEST_ORGIMPL_OPT
endif

ifdef MPUTEST_FIXED
    CFLAGS += -DMPUTEST_FIXED
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv8m/, string.o spinlock.o cpu.o hal.o \
  pmap.o exceptions.o _memcpy.o _memset.o interrupts.o)
