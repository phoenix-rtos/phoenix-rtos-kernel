#
# Makefile for Phoenix-RTOS kernel (SPARCV8 LEON3 HAL)
#
# Copyright 2023 Phoenix Systems
#
# %LICENSE%
#

include hal/$(TARGET_SUFF)/gaisler/$(TARGET_SUBFAMILY)/Makefile

CFLAGS := -Ihal/$(TARGET_SUFF)/gaisler/$(TARGET_SUBFAMILY) $(CFLAGS)

OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, ambapp.o console.o timer.o)

ifeq ($(TARGET_SUBFAMILY), gr712rc)
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, irqmp.o)
else ifeq ($(TARGET_SUBFAMILY), gr716)
  OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, irqamp.o)
endif
