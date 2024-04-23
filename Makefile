#
# Makefile for phoenix-rtos-kernel
#
# Copyright 2018, 2019 Phoenix Systems
#
# %LICENSE%
#

VERSION="3.2 rev: $(shell git rev-parse --short HEAD)"
CONSOLE?=vga
KERNEL=1

SIL ?= @
MAKEFLAGS += --no-print-directory

include ../phoenix-rtos-build/Makefile.common

CFLAGS += -I.
CPPFLAGS += -DVERSION=\"$(VERSION)\"

# uncomment to enable stack canary checking
# CPPFLAGS += -DSTACK_CANARY

EXTERNAL_HEADERS_DIR := ./include
EXTERNAL_HEADERS := $(shell find $(EXTERNAL_HEADERS_DIR) -name \*.h)

ifeq ($(TARGET_FAMILY),host)
  HEADERS_INSTALL_DIR := $(PREFIX_BUILD)/include/phoenix
else
  SYSROOT := $(shell $(CC) $(CFLAGS) -print-sysroot)
  HEADERS_INSTALL_DIR := $(SYSROOT)/usr/include/phoenix
  ifeq (/,$(SYSROOT))
    $(error Sysroot is not supported by toolchain. Use Phoenix-RTOS cross-toolchain to compile)
  endif
endif

OBJS := $(addprefix $(PREFIX_O), main.o syscalls.o syspage.o usrv.o)

all: $(PREFIX_PROG_STRIPPED)phoenix-$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).elf

ifneq ($(TARGET_FAMILY),host)
  include hal/Makefile
endif

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
	$(SIL)$(LD) $(CFLAGS) $(LDFLAGS) -nostdlib -e _start -Wl,--section-start,.init=$(VADDR_KERNEL_INIT) -o $@ $(OBJS) -lgcc -T ld/sparcv8leon3.ld
else
	$(SIL)$(LD) $(CFLAGS) $(LDFLAGS) -nostdlib -e _start -Wl,--section-start,.init=$(VADDR_KERNEL_INIT) -o $@ $(OBJS) -lgcc
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
