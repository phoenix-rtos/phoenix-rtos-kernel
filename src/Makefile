#
# Makefile for Phoenix-RTOS 3.0 kernel
#
# Copyright 2012-2015, 2016 Phoenix Systems
# Copyright 2001, 2005-2007, 2011 Pawel Pisarczyk
#
# %LICENSE%
#

SIL ?= @

#TARGET ?= ia32
#TARGET ?= armv7-stm32
#TARGET ?= armv7-imxrt
#TARGET ?= arm-imx
TARGET ?= riscv64

VERSION = 2.80
CORE_VERSION = 1.0
SRCDIR := $(CURDIR)
MAKEFLAGS += --no-print-directory --output-sync

SUBSYSTEMS = lib vm proc test

BIN = ../phoenix-$(TARGET)

ifeq ($(DEBUG), 1)
	CFLAGS = -O0 -g
else ifneq (, $(findstring imx, $(TARGET)))
	CFLAGS = -Os -DNDEBUG
else
	CFLAGS = -O2 -DNDEBUG
endif

# Compliation options for various architectures
include Makefile.ia32
include Makefile.armv7
include Makefile.arm
include Makefile.riscv64

ARCH = code.a
ARCHS := $(shell for i in $(SUBDIRS); do echo "$$i/$(ARCH)"; done)

export SIL CC CFLAGS MKDEP MKDEPFLAGS AR ARFLAGS ARCH TARGET OBJDUMP CONSOLE OBJCOPY


all: subsystems main.o syscalls.o $(EMBED) $(BIN)


.c.o:
	@(printf "CC  %-24s  " "$<"; $(CC) -c $(CFLAGS) $<)

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
	for i in `$(OBJDUMP) -t $$file | grep -e "F" | awk '{ print $$5 }'`; do \
		textsz=`echo $$(($$textsz + 0x$$i))`;\
	done;\
	printf " data=%-5d  text=%-5d  rodata=%-5d\n" $$datasz $$textsz $$rodatasz)


subsystems: $(ARCHS)

%/$(ARCH):
	@+echo "\033[1;32mCOMPILE $(@D)\033[0m";\
	if ! make -C "$(@D)"; then\
		exit 1;\
	fi;\


$(BIN): $(ARCHS) $(EMBED)
	@echo "\033[1;34mLD $@\033[0m"

	$(LD) $(LDFLAGS) -o $(BIN).elf main.o syscalls.o $(EMBED)  -\( $(ARCHS) $(LIBS) -\)
#	$(LD) $(LDFLAGS) -o $(BIN).elf $(EMBED)  main.o -\( $(ARCHS) $(LIBS) -\)
	$(SIL)$(OBJCOPY) -O binary $(BIN).elf $(BIN).img

	@(echo "";\
	echo "=> Phoenix-RTOS kernel binary for [$(TARGET)] has been created";\
	echo "")


install:
ifneq (, $(findstring armv7-stm32, $(TARGET)))
	@ocddir="/usr/local/share/openocd";\
	if [ -d "/usr/share/openocd" ]; then\
		ocddir="/usr/share/openocd";\
	fi;\
	openocd -f $$ocddir/scripts/interface/jlink.cfg -f $$ocddir/scripts/target/stm32l1.cfg -c "program $(BIN).elf verify reset exit";
endif


depend:
	@for i in $(SUBDIRS); do\
		d=`pwd`;\
		echo "DEPEND $$i";\
		if ! cd $$i; then\
			exit 1;\
		fi;\
		if ! $(MAKE) -s depend; then\
			exit 1;\
		fi;\
		cd $$d;\
	done;


clean:
	@rm -f core *.o $(BIN)
	@rm -f $(BIN).elf $(BIN).img $(BIN).hex
	@for i in $(SUBDIRS); do\
		d=`pwd`;\
		echo "CLEAN $$i";\
		if ! cd $$i; then\
			exit 1;\
		fi;\
		if ! $(MAKE) clean; then\
			exit 1;\
		fi;\
		cd $$d;\
	done;


.PHONY: clean
# DO NOT DELETE
