#
# Makefile for phoenix-rtos-kernel/trace
#
# Copyright 2025 Phoenix Systems
# Author: Adam Greloch
#

ifneq (,$(findstring arm7m,$(TARGET_FAMILY)))
 BUFFER_OBJ=buffer-rtt.o
else
 BUFFER_OBJ=buffer-mem.o
endif

OBJS += $(addprefix $(PREFIX_O)perf/, perf.o trace.o $(BUFFER_OBJ))
