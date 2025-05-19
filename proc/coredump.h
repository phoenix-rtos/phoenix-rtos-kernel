/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process coredump
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * %LICENSE%
 */

#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include "arch/exceptions.h"

typedef struct {
	int tid;
	short cursig;
	cpu_context_t *userContext;
} coredump_threadinfo_t;


extern void coredump_dump(unsigned int n, exc_context_t *ctx);

#endif
