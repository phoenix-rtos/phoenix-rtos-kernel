#
# Makefile for phoenix-rtos-kernel
#
# Copyright 2018, 2019 Phoenix Systems
#
# %LICENSE%
#

# GIT_DESC will be in format v<VERSION>-<#COMMITS SINCE TAG>-g<CURRENT COMMIT HASH>[ dirty]
DUMMY_VERSION := v3.3.1-0-g\#\#\#\#\#\#\#\#
GIT_DESC := $(shell git describe --dirty --abbrev=8 --tags --long --match "v[[:digit:]].[[:digit:]]*.[[:digit:]]*" 2> /dev/null || echo "$(DUMMY_VERSION)")
DESC := $(subst -, ,$(GIT_DESC))

VERSION := $(subst g,,$(word 3,$(DESC)))\ +$(word 2,$(DESC))
ifneq ($(word 4,$(DESC)),)
  VERSION := $(VERSION)\ $(word 4,$(DESC))
endif
RELEASE := $(subst v,,$(word 1,$(DESC)))

CONSOLE?=vga
KERNEL=1

SIL ?= @
MAKEFLAGS += --no-print-directory

include ../phoenix-rtos-build/Makefile.common

CFLAGS += -I. -ffreestanding
CPPFLAGS += -DVERSION=\"$(VERSION)\" -DRELEASE=\"$(RELEASE)\" -DTARGET_FAMILY=\"$(TARGET_FAMILY)\"

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
include perf/Makefile

# incremental build quick-fix, WARN: assuming the sources are in c
DEPS := $(patsubst %.o, %.c.d, $(OBJS))
-include $(DEPS)


# By default ld adds program headers to first LOAD segment.
# However, in kernel headers are not used and it is expected that kernel address space starts at _start.
# This target creates linker script that do not add program headers into the first loadable segment.
# 1) Get internal linker script with prefix and suffix containing noise.
# 2) Match only internal linker script which is between two lines of =
# 3) Remove those lines
# 4) Remove "+ SIZEOF_HEADERS", which causes inclusion of program headers.
$(PREFIX_O)$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).ldt:
	$(SIL)$(LD) $(LDFLAGS_PREFIX)--verbose 2>/dev/null | sed -n '/^==*$$/,/^==*$$/p' | sed '1,1d; $$d' | sed s/"\s*+\s*SIZEOF_HEADERS"// > "$@"


$(PREFIX_PROG)phoenix-$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).elf: $(OBJS) $(PREFIX_O)$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).ldt
	@mkdir -p $(@D)
	@(printf "LD  %-24s\n" "$(@F)");
	$(SIL)$(LD) $(CFLAGS) $(LDFLAGS) -nostdlib -e _start -Wl,--section-start,.init=$(VADDR_KERNEL_INIT) -o $@ $(OBJS) -lgcc -T $(PREFIX_O)$(TARGET_FAMILY)-$(TARGET_SUBFAMILY).ldt

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
