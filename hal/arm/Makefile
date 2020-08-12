#
# Makefile for Phoenix-RTOS kernel (ARM HAL)
#
# Copyright 2018, 2020 Phoenix Systems
#


OBJS += $(addprefix $(PREFIX_O)hal/arm/, hal.o pmap.o spinlock.o syspage.o cpu.o interrupts.o exceptions.o timer.o)


ifneq (, $(findstring imx, $(TARGET)))
	OBJS += $(PREFIX_O)hal/arm/_init-imx6ull.o $(PREFIX_O)hal/arm/console-imx6ull.o $(PREFIX_O)hal/arm/imx6ull.o
endif

#memtest: _memtest.o memtest.o
#	@arm-phoenix-ld -o memtest.elf -e _start --section-start .init=0x907000 -z max-page-size=0x1000 _memtest.o memtest.o
#	@$(OBJCOPY) memtest.elf -Obinary memtest.img

