#
# Makefile for Phoenix-RTOS kernel (sparcv8leon3 HAL)
#
# Copyright 2022 Phoenix Systems
#

include hal/$(TARGET_SUFF)/gaisler/Makefile
include hal/gaisler/Makefile

CFLAGS := -Ihal/$(TARGET_SUFF) $(CFLAGS)

OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/, cpu.o hal.o spinlock.o string.o _traps.o)

ifeq ($(findstring -DNOMMU,$(CPPFLAGS)),-DNOMMU)
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/, exceptions-nommu.o pmap-nommu.o _interrupts-nommu.o)
else
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/, exceptions.o pmap.o srmmu.o tlb.o _interrupts.o)
endif
