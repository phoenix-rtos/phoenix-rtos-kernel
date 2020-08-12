#
# Makefile for Phoenix-RTOS kernel (lib)
#
# Copyright 2012-2015, 2016-2017 Phoenix Systems
# Copyright 2001, 2005-2006 Pawel Pisarczyk
#

SRCS = printf.c bsearch.c rand.c strtoul.c rb.c list.c cbuffer.c


OBJS = $(SRCS:.c=.o)


all: check $(ARCH)


check:
	@(if test -z "$(ARCH)"; then\
		echo "Error: Only main Makefile can be used for compilation!";\
		exit 1;\
	fi;)


.c.o:
	@(printf "CC  lib/%-24s\n" "$<"; $(CC) -c $(CFLAGS) $< ;)


-include .depend


$(ARCH): $(OBJS)
	$(SIL)$(AR) $(ARFLAGS) $(ARCH) $(OBJS) 2>/dev/null


depend: check
	$(SIL)$(MKDEP) $(MKDEPFLAGS) $(SRCS) >.depend


clean: check
	$(SIL)rm -f core *.o $(ARCH)
