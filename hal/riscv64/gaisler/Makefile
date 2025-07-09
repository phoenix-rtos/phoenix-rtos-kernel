#
# Makefile for Phoenix-RTOS kernel (SPARCV8 LEON3 HAL)
#
# Copyright 2023-2025 Phoenix Systems
#
# %LICENSE%
#

include hal/$(TARGET_SUFF)/gaisler/$(TARGET_SUBFAMILY)/Makefile

CFLAGS += -Ihal/$(TARGET_SUFF)/gaisler/$(TARGET_SUBFAMILY)

OBJS += $(addprefix $(PREFIX_O)hal/$(TARGET_SUFF)/gaisler/, console.o)
