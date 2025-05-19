#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include "arch/exceptions.h"

typedef struct {
	int tid;
	unsigned cursig;
	unsigned sigmask;
	unsigned sigpend;
	cpu_context_t *userContext;
} coredump_threadinfo_t;


void coredump_dump(unsigned int n, exc_context_t *ctx);

#endif
