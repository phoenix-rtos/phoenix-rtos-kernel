#
# Makefile for Phoenix-RTOS kernel (ARM HAL)
#
# Copyright 2018, 2020, 2021 Phoenix Systems
#

ifneq (, $(findstring zynq7000, $(TARGET_SUBFAMILY)))
    include hal/armv7a/zynq7000/Makefile
    CFLAGS += -Ihal/armv7a/zynq7000 -Ihal/armv7a
else ifneq (, $(findstring imx6ull, $(TARGET_SUBFAMILY)))
    include hal/armv7a/imx6ull/Makefile
    CFLAGS += -Ihal/armv7a/imx6ull -Ihal/armv7a
endif

OBJS += $(addprefix $(PREFIX_O)hal/armv7a/, spinlock.o cpu.o hal.o exceptions.o)


# HACKISH: different OBJS for memtest target
.PHONY: memtest
memtest: $(PREFIX_PROG)/memtest.elf $(PREFIX_PROG)/memtest.img
memtest: OBJS:=$(addprefix $(PREFIX_O)hal/armv7a/, _memtest.o memtest.o)

$(PREFIX_PROG)/memtest.elf: $(PREFIX_O)hal/armv7a/_memtest.o $(PREFIX_O)hal/armv7a/memtest.o
	$(SIL)$(LD) $(LDFLAGS) -o $@ -e _start --section-start .init=0x907000 -z max-page-size=0x1000 $^

$(PREFIX_PROG)/memtest.img: $(PREFIX_PROG)/memtest.elf
	$(SIL)$(OBJCOPY) $< -Obinary $@
