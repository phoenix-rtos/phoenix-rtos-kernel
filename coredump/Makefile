#
# Makefile for phoenix-rtos-kernel/coredump
#
# Copyright 2025 Phoenix Systems
# Author: Jakub Klimek
#


ifeq ($(COREDUMP_DISABLE), 1)
	CPPFLAGS += -DCOREDUMP_DISABLE
else
	OBJS += $(addprefix $(PREFIX_O)coredump/, coredump.o)
endif
