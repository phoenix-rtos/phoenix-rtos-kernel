#
# Makefile for phoenix-rtos-kernel/posix
#
# Copyright 2020 Phoenix Systems
# Author: Pawel Pisarczyk
#

ifdef KERNEL_TEST_GROUPS
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
COMMA := ,
KERNEL_TEST_GROUPS := $(subst $(COMMA),$(SPACE),$(KERNEL_TEST_GROUPS))

CFLAGS += -DKERNEL_TEST
CFLAGS += -I$(PREFIX_H)unity

UNITYLIB := $(PREFIX_A)unity.a

OBJS += $(PREFIX_O)test/test_main.o

include $(addprefix test/, $(addsuffix /Makefile, $(KERNEL_TEST_GROUPS)))
endif

OBJS += $(addprefix $(PREFIX_O)test/, test.o vm.o rb.o proc.o msg.o)

