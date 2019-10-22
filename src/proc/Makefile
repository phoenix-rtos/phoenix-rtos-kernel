#
# Makefile for Phoenix-RTOS kernel (vm)
#
# Copyright 2012-2015, 2017, 2018 Phoenix Systems
# Copyright 2001, 2005-2006 Pawel Pisarczyk
#

SRCS = proc.c threads.c process.c name.c resource.c mutex.c cond.c userintr.c file.c ports.c server.c group.c event.c socket.c pipe.c

ifneq (, $(findstring NOMMU, $(CFLAGS)))
	SRCS += msg-nommu.c
else
	SRCS += msg.c
endif

OBJS = $(SRCS:.c=.o)


all: check $(ARCH)


check:
	@(if test -z "$(ARCH)"; then\
		echo "Error: Only main Makefile can be used for compilation!";\
		exit 1;\
	fi;)


.c.o:
	@(printf "CC  proc/%-24s\n" "$<"; $(CC) -c $(CFLAGS) $< ; )

-include .depend


$(ARCH): $(OBJS)
	$(SIL)$(AR) $(ARFLAGS) $(ARCH) $(OBJS) 2>/dev/null


depend: check
	$(SIL)$(MKDEP) $(MKDEPFLAGS) $(SRCS) >.depend


clean: check
	$(SIL)rm -f core *.o $(ARCH)


.PHONY: clean
# DO NOT DELETE
