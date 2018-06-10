/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_EXCEPTIONS_H_
#define _HAL_EXCEPTIONS_H_

#ifndef __ASSEMBLY__

#include "cpu.h"


#define EXC_DEFAULT    128

#define EXC_UNDEFINED  3

#define SIZE_CTXDUMP   512    /* Size of dumped context */


typedef struct _exc_context_t {
	/* Saved by ISR */
	u32 psp;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;
	u32 r11;

	/* Saved by hardware */
	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r12;
	u32 lr;
	u32 pc;
	u32 psr;
} exc_context_t;


extern void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n);


static inline int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
{
	return 0;
}


extern void exceptions_dispatch(unsigned int n, exc_context_t *ctx);


#endif

#endif
