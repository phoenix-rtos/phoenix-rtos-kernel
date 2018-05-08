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


ASMS += _memtest.S
SRCS += hal.c pmap.c spinlock.c syspage.c cpu.c interrupts.c exceptions.c timer.c memtest.c
OBJS = $(ASMS:.S=.o) $(SRCS:.c=.o)


all: check memtest $(ARCH)


check:
	@(if test -z "$(ARCH)"; then\
		echo "Error: Only main Makefile can be used for compilation!";\
		exit 1;\
	fi;)


.c.o:
	@(printf " CC  %-24s  " "$<"; $(CC) -c $(CFLAGS) $<)

	@(file=`echo $< | sed "s/\.c/\.o/"`; \
	datasz=0;\
	textsz=0;\
	rodatasz=0;\
	for i in `$(OBJDUMP) -t $$file | grep -e " O " | grep -v ".rodata" | awk '{ print $$4 }'`; do \
		datasz=`echo $$(($$datasz + 0x$$i))`;\
	done; \
	for i in `$(OBJDUMP) -t $$file | grep -e " O " | grep ".rodata" | awk '{ print $$5 }'`; do \
                rodatasz=`echo $$(($$rodatasz + 0x$$i))`;\
        done; \
	for i in `$(OBJDUMP) -t $$file | grep -e " F " | awk '{ print $$5 }'`; do \
		textsz=`echo $$(($$textsz + 0x$$i))`;\
	done;\
	printf "data=%-5d  text=%-5d  rodata=%-5d\n" $$datasz $$textsz $$rodatasz)


.S.o:
	@(printf " ASM %-24s  " "$<"; $(CC) -c $(CFLAGS) $<)

	@(file=`echo $< | sed "s/\.S/\.o/"`; \
	datasz=0;\
	textsz=0;\
	for i in `$(OBJDUMP) -t $$file | grep -e " O " | awk '{ print $$5 }'`; do \
		datasz=`echo $$(($$datasz + 0x$$i))`;\
	done; \
	for i in `$(OBJDUMP) -t $$file | grep -e " F " | awk '{ print $$5 }'`; do \
		textsz=`echo $$(($$textsz + 0x$$i))`;\
	done;\
	printf "data=%-5d  text=%-5d\n" $$datasz $$textsz)


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
