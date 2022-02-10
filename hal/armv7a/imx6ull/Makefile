#
# Makefile for Phoenix-RTOS kernel (ARMv7A HAL imx6ull)
#
# Copyright 2021 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/armv7a/imx6ull/, _init.o console.o imx6ull.o timer.o interrupts.o)

$(PREFIX_O)hal/armv7a/imx6ull/_init.o: hal/armv7a/_armv7a.S hal/armv7a/_interrupts.S

ifdef MEMTEST_SHORT
	CFLAGS += -DMEMTEST_SHORT
endif

# TODO: remove after introducing plo
# HACKISH: different OBJS for memtest target
.PHONY: memtest
memtest: $(PREFIX_PROG)/memtest.elf $(PREFIX_PROG)/memtest.img
memtest: OBJS:=$(addprefix $(PREFIX_O)hal/armv7a/imx6ull/, _memtest.o memtest.o)

$(PREFIX_PROG)/memtest.elf: $(PREFIX_O)hal/armv7a/imx6ull/_memtest.o $(PREFIX_O)hal/armv7a/imx6ull/memtest.o
	$(SIL)$(LD) $(LDFLAGS) -o $@ -e _start --section-start .init=0x907000 -z max-page-size=0x1000 $^

$(PREFIX_PROG)/memtest.img: $(PREFIX_PROG)/memtest.elf
	$(SIL)$(OBJCOPY) $< -Obinary $@
