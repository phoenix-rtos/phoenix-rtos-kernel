#
# Makefile for libphoenix/arch/ia32
#
# Copyright 2017, 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

#ASMS = _init.S _exceptions.S _interrupts.S
#SRCS = syspage.c multiboot.c spinlock.c exceptions.c interrupts.c cpu.c pmap.c timer.c hal.c string.c
#watchdog.c

OBJS += $(addprefix $(PREFIX_O)vm/, vm.o map.o zone.o kmalloc.o object.o amap.o)

ifneq (, $(findstring NOMMU, $(CFLAGS)))
	OBJS += $(PREFIX_O)vm/page-nommu.o
else
	OBJS += $(PREFIX_O)vm/page.o
endif
