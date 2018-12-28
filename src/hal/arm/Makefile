#
# Makefile for Phoenix-RTOS kernel (ARM HAL)
#
# Copyright 2018 Phoenix Systems
#


ifneq (, $(findstring imx, $(TARGET)))
	ASMS = _init-imx6ull.S
	SRCS = console-imx6ull.c imx6ull.c
#imx.c console-imx.c interrupts-imx.c timer-imx.c syspage-imx.c
endif


SRCS += hal.c pmap.c spinlock.c syspage.c cpu.c interrupts.c exceptions.c timer.c
OBJS = $(ASMS:.S=.o) $(SRCS:.c=.o)


all: check $(ARCH)


check:
	@(if test -z "$(ARCH)"; then\
		echo "Error: Only main Makefile can be used for compilation!";\
		exit 1;\
	fi;)


.c.o:
	@(printf "CC  hal/arm/%-24s\n" "$<"; $(CC) -c $(CFLAGS) $< )

.S.o:
	@(printf "ASM hal/arm/%-24s\n" "$<"; $(CC) -c $(CFLAGS) $< )

-include .depend


$(ARCH): $(OBJS)
	@$(AR) $(ARFLAGS) $(ARCH) $(OBJS) 2>/dev/null


memtest: _memtest.o memtest.o
	@arm-phoenix-ld -o memtest.elf -e _start --section-start .init=0x907000 -z max-page-size=0x1000 _memtest.o memtest.o
	@$(OBJCOPY) memtest.elf -Obinary memtest.img

depend: check
	@$(MKDEP) $(MKDEPFLAGS) $(SRCS) >.depend


clean: check
	@rm -f core *.elf *.img *.o $(ARCH)
