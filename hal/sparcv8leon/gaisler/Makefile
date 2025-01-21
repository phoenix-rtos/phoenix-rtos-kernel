#
# Makefile for Phoenix-RTOS kernel (SPARCV8 LEON3 HAL)
#
# Copyright 2023-2025 Phoenix Systems
#
# %LICENSE%
#

include hal/$(TARGET_SUFF)/gaisler/$(TARGET_SUBFAMILY)/Makefile

CFLAGS := -Ihal/$(TARGET_SUFF)/gaisler/$(TARGET_SUBFAMILY) $(CFLAGS)

OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, console.o timer.o)

ifeq ($(TARGET_SUBFAMILY), gr712rc)
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, irqmp.o)
else ifeq ($(TARGET_SUBFAMILY), gr716)
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, irqamp.o)
else ifeq ($(TARGET_SUBFAMILY), gr740)
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, irqamp.o l2cache.o)
else ifeq ($(TARGET_SUBFAMILY), generic)
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, irqmp.o)
endif
