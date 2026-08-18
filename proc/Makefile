#
# Makefile for phoenix-rtos-kernel/proc
#
# Copyright 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

OBJS += $(addprefix $(PREFIX_O)proc/, proc.o threads.o process.o name.o resource.o mutex.o cond.o userintr.o ports.o)

# TODO: rename back to msg
ifneq (, $(findstring NOMMU, $(CPPFLAGS)))
        OBJS += $(PREFIX_O)proc/xfer-nommu.o
else
        OBJS += $(PREFIX_O)proc/xfer.o
endif
