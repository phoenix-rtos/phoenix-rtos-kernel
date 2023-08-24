#
# Makefile for Phoenix-RTOS kernel (ARMv7-A HAL Zynq-7000)
#
# Copyright 2021 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/armv7a/zynq7000/, _init.o console.o zynq.o timer.o interrupts.o)

$(PREFIX_O)hal/armv7a/zynq7000/_init.o: hal/armv7a/_armv7a.S hal/armv7a/_interrupts.S
