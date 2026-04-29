#
# Makefile for Phoenix-RTOS kernel (ARMv8M HAL)
#
# Copyright 2022 Phoenix Systems
#


ifneq (, $(findstring nrf, $(TARGET_SUBFAMILY)))
    include hal/armv8m/nrf/Makefile
else ifneq (, $(findstring mcx, $(TARGET_SUBFAMILY)))
    include hal/armv8m/mcx/Makefile
else ifneq (, $(findstring stm32, $(TARGET_SUBFAMILY)))
    include hal/armv8m/stm32/Makefile
endif

include hal/arm/Makefile

CFLAGS += -Ihal/armv8m -Ihal/arm -DKERNEL_SPINLOCK_ARM_M=1

OBJS += $(addprefix $(PREFIX_O)hal/arm/, spinlock.o)
OBJS += $(addprefix $(PREFIX_O)hal/armv8m/, string.o cpu.o hal.o \
  pmap.o exceptions.o _memcpy.o _memset.o interrupts.o)
