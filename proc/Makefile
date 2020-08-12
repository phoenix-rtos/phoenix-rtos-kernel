#
# Makefile for phoenix-rtos-kernel/proc
#
# Copyright 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

OBJS += $(addprefix $(PREFIX_O)proc/, proc.o threads.o process.o name.o resource.o mutex.o cond.o userintr.o file.o ports.o)

ifneq (, $(findstring NOMMU, $(CFLAGS)))
        OBJS += $(PREFIX_O)proc/msg-nommu.o
else
        OBJS += $(PREFIX_O)proc/msg.o
endif
