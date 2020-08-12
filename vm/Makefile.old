#
# Makefile for Phoenix-RTOS kernel (vm)
#
# Copyright 2012-2015, 2016-2017 Phoenix Systems
# Copyright 2001, 2005-2006 Pawel Pisarczyk
#

SRCS = vm.c map.c zone.c kmalloc.c object.c amap.c

ifneq (, $(findstring NOMMU, $(CFLAGS)))
	SRCS += page-nommu.c
else
	SRCS += page.c
endif


OBJS = $(SRCS:.c=.o)


all: check $(ARCH)


check:
	@(if test -z "$(ARCH)"; then\
		echo "Error: Only main Makefile can be used for compilation!";\
		exit 1;\
	fi;)


.c.o:
	@(printf "CC  vm/%-24s\n" "$<"; $(CC) -c $(CFLAGS) $<;)


-include .depend


$(ARCH): $(OBJS)
	$(SIL)$(AR) $(ARFLAGS) $(ARCH) $(OBJS) 2>/dev/null


depend: check
	$(SIL)$(MKDEP) $(MKDEPFLAGS) $(SRCS) >.depend


clean: check
	$(SIL)rm -f core *.o $(ARCH)


.PHONY: clean
# DO NOT DELETE
