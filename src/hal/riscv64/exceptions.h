/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_EXCEPTIONS_H_
#define _HAL_EXCEPTIONS_H_

#include "cpu.h"
#include "../../../include/mman.h"


#define EXC_DEFAULT    128

#define EXC_UNDEFINED  0
//#define EXC_PAGEFAULT  0


typedef cpu_context_t exc_context_t;


static inline int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	return PROT_NONE;
}


static inline void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return NULL;
}


extern void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n);


extern int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *));


extern void _hal_exceptionsInit(void);


#endif
