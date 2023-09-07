#
# Makefile for phoenix-rtos-kernel/vm
#
# Copyright 2017, 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

OBJS += $(addprefix $(PREFIX_O)vm/, vm.o map.o zone.o kmalloc.o object.o amap.o)

ifneq (, $(findstring NOMMU, $(CPPFLAGS)))
	OBJS += $(PREFIX_O)vm/page-nommu.o
else
	OBJS += $(PREFIX_O)vm/page.o
endif
