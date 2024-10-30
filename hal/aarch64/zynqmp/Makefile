#
# Makefile for Phoenix-RTOS kernel (ARMv8-A HAL ZynqMP)
#
# Copyright 2021, 2024 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/aarch64/zynqmp/, _init.o console.o zynqmp.o timer.o)

$(PREFIX_O)hal/aarch64/zynqmp/_init.o: hal/aarch64/_aarch64v8a.S hal/aarch64/_interrupts.S
