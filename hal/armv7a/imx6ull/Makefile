#
# Makefile for Phoenix-RTOS kernel (ARMv7-A HAL i.MX 6ULL)
#
# Copyright 2021 Phoenix Systems
#

OBJS += $(addprefix $(PREFIX_O)hal/armv7a/imx6ull/, _init.o console.o imx6ull.o timer.o interrupts.o)

$(PREFIX_O)hal/armv7a/imx6ull/_init.o: hal/armv7a/_armv7a.S hal/armv7a/_interrupts.S

ifdef MEMTEST_SHORT
  CPPFLAGS += -DMEMTEST_SHORT
endif

ifeq ($(KERNEL_PLO_BOOT), y)
  CPPFLAGS += -DKERNEL_PLO_BOOT
endif

# TODO: remove after introducing plo
# HACKISH: different OBJS for memtest target
.PHONY: memtest
memtest: $(PREFIX_PROG)/memtest.elf $(PREFIX_PROG)/memtest.img
memtest: OBJS:=$(addprefix $(PREFIX_O)hal/armv7a/imx6ull/, _memtest.o memtest.o)

$(PREFIX_PROG)/memtest.elf: $(PREFIX_O)hal/armv7a/imx6ull/_memtest.o $(PREFIX_O)hal/armv7a/imx6ull/memtest.o
	$(SIL)$(LD) $(CFLAGS) $(LDFLAGS) -o $@ -nostdlib -e _start -Wl,--section-start,.init=0x907000 -z max-page-size=0x1000 $^ -lgcc

$(PREFIX_PROG)/memtest.img: $(PREFIX_PROG)/memtest.elf
	$(SIL)$(OBJCOPY) $< -Obinary $@
