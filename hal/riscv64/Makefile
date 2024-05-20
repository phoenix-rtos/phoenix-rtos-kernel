#
# Makefile for Phoenix-RTOS kernel (RISCV64 HAL)
#
# Copyright 2018, 2020, 2024 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/riscv64/, _init.o _string.o _interrupts.o hal.o spinlock.o interrupts.o cpu.o pmap.o \
dtb.o timer.o string.o exceptions.o plic.o sbi.o)

CFLAGS += -Ihal/riscv64

OBJS += $(PREFIX_O)hal/riscv64/console.o

