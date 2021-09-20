#
# Makefile for Phoenix-RTOS kernel (ARMv7A HAL imx6ull)
#
# Copyright 2021 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/armv7a/imx6ull/, _init.o console.o imx6ull.o timer.o interrupts.o)

$(PREFIX_O)hal/armv7a/imx6ull/_init.o: hal/armv7a/_hal.S hal/armv7a/_interrupts.S
