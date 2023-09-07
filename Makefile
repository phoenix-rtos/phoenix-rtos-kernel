#
# Makefile for phoenix-rtos-kernel
#
# Copyright 2018, 2019 Phoenix Systems
#
# %LICENSE%
#

VERSION="3.0 rev: $(shell git rev-parse --short HEAD)"
CONSOLE?=vga
KERNEL=1

SIL ?= @
MAKEFLAGS += --no-print-directory

include ../phoenix-rtos-build/Makefile.common

# TODO: replace BOARD_CONFIG usage with board_config.h
CFLAGS += $(BOARD_CONFIG)
CFLAGS += -I.
CFLAGS += -DVERSION=\"$(VERSION)\"

# uncomment to enable stack canary checking
# CFLAGS += -DSTACK_CANARY

EXTERNAL_HEADERS_DIR := ./include
EXTERNAL_HEADERS := $(shell find $(EXTERNAL_HEADERS_DIR) -name \*.h)

SYSROOT := $(shell $(CC) $(CFLAGS) -print-sysroot)
HEADERS_INSTALL_DIR := $(SYSROOT)/usr/include/phoenix
ifeq (/,$(SYSROOT))
  $(error Sysroot is not supported by toolchain. Use Phoenix-RTOS cross-toolchain to compile)
endif

OBJS := $(addprefix $(PREFIX_O), main.o syscalls.o syspage.o usrv.o)

all: $(PREFIX_PROG_STRIPPED)phoenix-$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).elf

include hal/$(TARGET_SUFF)/Makefile
include vm/Makefile
include proc/Makefile
include posix/Makefile
include lib/Makefile
include test/Makefile
include log/Makefile

# incremental build quick-fix, WARN: assuming the sources are in c
DEPS := $(patsubst %.o, %.c.d, $(OBJS))
-include $(DEPS)

$(PREFIX_PROG)phoenix-$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).elf: $(OBJS)
	@mkdir -p $(@D)
	@(printf "LD  %-24s\n" "$(@F)");
ifeq ($(TARGET_FAMILY),sparcv8leon3)
	$(SIL)$(LD) $(LDFLAGS) -e _start --section-start .init=$(VADDR_KERNEL_INIT) -o $@ $(OBJS) $(GCCLIB) -T ld/sparcv8leon3.ld
else
	$(SIL)$(LD) $(LDFLAGS) -e _start --section-start .init=$(VADDR_KERNEL_INIT) -o $@ $(OBJS) $(GCCLIB)
endif

install-headers: $(EXTERNAL_HEADERS)
	@printf "Installing kernel headers\n"
	@mkdir -p "$(HEADERS_INSTALL_DIR)"; \
	for file in $(EXTERNAL_HEADERS); do\
		mkdir -p "$(HEADERS_INSTALL_DIR)/`dirname $${file#$(EXTERNAL_HEADERS_DIR)}`"; \
		install -p -m 644 $${file} "$(HEADERS_INSTALL_DIR)/$${file#$(EXTERNAL_HEADERS_DIR)}";\
	done


uninstall-headers:
	rm -rf "$(HEADERS_INSTALL_DIR)"/*

.PHONY: install-headers uninstall-headers all
