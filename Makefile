#
# Makefile for phoenix-rtos-kernel
#
# Copyright 2018, 2019 Phoenix Systems
#
# %LICENSE%
#

VERSION="2.97 rev: "`git rev-parse --short HEAD`
CONSOLE=vga

SIL ?= @
MAKEFLAGS += --no-print-directory

#TARGET ?= armv7m3-stm32l152xd
#TARGET ?= armv7m3-stm32l152xe
#TARGET ?= armv7m4-stm32l4x6
#TARGET ?= armv7m7-imxrt105x
#TARGET ?= armv7m7-imxrt106x
TARGET ?= armv7m7-imxrt117x
#TARGET ?= armv7a7-imx6ull
#TARGET ?= ia32-generic
#TARGET ?= riscv64-spike

include ../phoenix-rtos-build/Makefile.common
include ../phoenix-rtos-build/Makefile.$(TARGET_SUFF)

CFLAGS += -I. -DHAL=\"hal/$(TARGET_SUFF)/hal.h\" -DVERSION=\"$(VERSION)\"

OBJS = $(PREFIX_O)main.o $(PREFIX_O)syscalls.o


all: $(PREFIX_PROG)phoenix-$(TARGET)


include hal/$(TARGET_SUFF)/Makefile
include vm/Makefile
include proc/Makefile
include posix/Makefile
include lib/Makefile
include test/Makefile


$(BUILD_DIR)/programs.cpio:
	@printf "TOUCH programs.cpio\n"
	$(SIL)touch $(BUILD_DIR)/programs.cpio


$(PREFIX_O)/programs.o.cpio: $(PREFIX_O)programs.o $(BUILD_DIR)/programs.cpio
	@printf "EMBED programs.cpio\n"
	$(SIL)$(OBJCOPY) --update-section .data=$(BUILD_DIR)/programs.cpio $(PREFIX_O)programs.o --add-symbol programs=.data:0 $(PREFIX_O)programs.o.cpio


$(PREFIX_PROG)phoenix-$(TARGET): $(OBJS) $(PREFIX_O)/programs.o.cpio	
	@(printf "LD  %-24s\n" "$(@F)");
	$(SIL)$(LD) $(LDFLAGS) -e _start --section-start .init=$(VADDR_KERNEL_INIT) -o $(PREFIX_PROG)phoenix-$(TARGET).elf $(OBJS) $(PREFIX_O)/programs.o.cpio $(GCCLIB)


.PHONY: clean headers install
clean:
	@echo "rm -rf $(BUILD_DIR)"

ifneq ($(filter clean,$(MAKECMDGOALS)),)
	$(shell rm -rf $(BUILD_DIR))
	$(shell rm -f string/*.inc)
endif
