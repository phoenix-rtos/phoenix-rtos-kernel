#
# Makefile for phoenix-rtos-kernel/hal/ia32
#
# Copyright 2017, 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

OBJS += $(addprefix $(PREFIX_O)hal/ia32/, _init.o _exceptions.o _interrupts.o multiboot.o spinlock.o exceptions.o interrupts.o cpu.o pmap.o timer.o hal.o string.o pci.o tlb.o init.o)
CFLAGS += -Ihal/ia32

ifeq ($(CONSOLE), vga)
	OBJS += $(PREFIX_O)hal/ia32/console.o
else
	OBJS += $(PREFIX_O)hal/ia32/console-serial.o
endif
