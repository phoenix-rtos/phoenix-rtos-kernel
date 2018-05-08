#
# Makefile for Phoenix-RTOS kernel (IA32 HAL)
#
# Copyright 2012-2015, 2016 Phoenix Systems
# Copyright 2001, 2005-2006 Pawel Pisarczyk
#

ASMS = _init.S _exceptions.S _interrupts.S
SRCS = syspage.c multiboot.c spinlock.c exceptions.c interrupts.c cpu.c pmap.c timer.c hal.c
#watchdog.c

ifeq ($(CONSOLE), vga)
	SRCS += console.c
else
	SRCS += console-serial.c
endif


OBJS = $(ASMS:.S=.o) $(SRCS:.c=.o) 


all: check $(ARCH)


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
	for i in `$(OBJDUMP) -t $$file | grep -e "O" | grep -v ".rodata" | awk '{ print $$4 }'`; do \
		datasz=`echo $$(($$datasz + 0x$$i))`;\
	done; \
	for i in `$(OBJDUMP) -t $$file | grep -e " O " | grep ".rodata" | awk '{ print $$5 }'`; do \
                rodatasz=`echo $$(($$rodatasz + 0x$$i))`;\
        done; \
	for i in `$(OBJDUMP) -t $$file | grep -e "F" | awk '{ print $$5 }'`; do \
		textsz=`echo $$(($$textsz + 0x$$i))`;\
	done;\
	printf "data=%-5d  text=%-5d  rodata=%-5d\n" $$datasz $$textsz $$rodatasz)


.S.o:
	@(printf " ASM %-24s  " "$<"; $(CC) -c $(CFLAGS) $<)

	@(file=`echo $< | sed "s/\.S/\.o/"`; \
	datasz=0;\
	textsz=0;\
	for i in `$(OBJDUMP) -t $$file | grep -e "O" | awk '{ print $$5 }'`; do \
		datasz=`echo $$(($$datasz + 0x$$i))`;\
	done; \
	for i in `$(OBJDUMP) -t $$file | grep -e "F" | awk '{ print $$5 }'`; do \
		textsz=`echo $$(($$textsz + 0x$$i))`;\
	done;\
	printf "data=%-5d  text=%-5d\n" $$datasz $$textsz)


-include .depend


$(ARCH): $(OBJS)
	$(SIL)$(AR) $(ARFLAGS) $(ARCH) $(OBJS) 2>/dev/null


depend: check
	$(SIL)$(MKDEP) $(MKDEPFLAGS) $(SRCS) >.depend

clean: check
	$(SIL)rm -f core *.o $(ARCH)
