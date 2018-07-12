#
# Makefile for Phoenix-RTOS 3.0 kernel
#
# Copyright 2012-2015, 2016-2018 Phoenix Systems
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
SRCS := main.c syscalls.c programs.c
OBJS = $(SRCS:.c=.o)

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
TARGET_FAMILY = $(firstword $(subst -, ,$(TARGET)-))
include Makefile.$(TARGET_FAMILY)

ARCH = code.a
ARCHS := $(shell for i in $(SUBDIRS); do echo "$$i/$(ARCH)"; done)

EXTERNAL_HEADERS_DIR := ../include
EXTERNAL_HEADERS := $(shell find $(EXTERNAL_HEADERS_DIR) -name \*.h)

SYSROOT := $(shell $(CC) $(CFLAGS) -print-sysroot)
HEADERS_INSTALL_DIR := $(SYSROOT)/usr/local/include/phoenix
ifeq (/,$(SYSROOT))
$(error Sysroot is not supported by Your toolchain. Use cross-toolchain to compile)
endif

export SIL CC CFLAGS MKDEP MKDEPFLAGS AR ARFLAGS ARCH TARGET OBJDUMP CONSOLE OBJCOPY


all: subsystems $(OBJS) $(EMBED) $(BIN)


.c.o:
	@(printf "CC  %-24s  " "$<"; $(CC) -c $(CFLAGS) $<)

	@(file="$@"; \
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



$(OBJS): $(filter clean,$(MAKECMDGOALS))


subsystems: $(ARCHS)


%/$(ARCH): .FORCE $(filter clean,$(MAKECMDGOALS))
	@+echo "\033[1;32mCOMPILE $(@D)\033[0m";\
	if ! $(MAKE) -C "$(@D)"; then\
		exit 1;\
	fi;\


$(BIN): $(ARCHS) $(EMBED) $(OBJS)
	@echo "\033[1;34mEMBED programs.cpio\033[0m"
	@touch /tmp/programs
	$(SIL)$(OBJCOPY) --update-section .data=/tmp/programs programs.o --add-symbol programs=.data:0

	@echo "\033[1;34mLD $@\033[0m"
	$(LD) $(LDFLAGS) -o $(BIN).elf $(OBJS) $(EMBED) -\( $(ARCHS) $(LIBS) -\)
	$(SIL)$(OBJCOPY) -O binary $(BIN).elf $(BIN).img

	@rm ./programs.o

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


install-headers: $(EXTERNAL_HEADERS)
	@mkdir -p "$(HEADERS_INSTALL_DIR)"; \
	for file in $(EXTERNAL_HEADERS); do\
		mkdir -p "$(HEADERS_INSTALL_DIR)/`dirname $${file#$(EXTERNAL_HEADERS_DIR)}`"; \
		install -p -m 644 $${file} "$(HEADERS_INSTALL_DIR)/$${file#$(EXTERNAL_HEADERS_DIR)}";\
	done


uninstall-headers:
	rm -rf "$(HEADERS_INSTALL_DIR)"/*


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
		echo "CLEAN $$i";\
		if ! $(MAKE) -C "$$i" clean; then\
			exit 1;\
		fi;\
	done;


.FORCE:


.PHONY: clean
# DO NOT DELETE
