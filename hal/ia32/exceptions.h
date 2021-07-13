/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_EXCEPTIONS_H_
#define _HAL_EXCEPTIONS_H_

#ifndef __ASSEMBLY__

#include "cpu.h"
#include "../../include/mman.h"


#define EXC_DEFAULT    128

#define EXC_UNDEFINED  6
#define EXC_PAGEFAULT  14

#define SIZE_CTXDUMP   512    /* Size of dumped context */


#pragma pack(push, 1)

typedef struct {
	u32 dr0;
	u32 dr1;
	u32 dr2;
	u32 dr3;
	u32 dr4;
	u32 dr5;
	u32 edi;
	u32 esi;
	u32 ebp;
	u32 edx;
	u32 ecx;
	u32 ebx;
	u32 eax;
	u16 gs;
	u16 fs;
	u16 es;
	u16 ds;
	u32 err;
	u32 eip;
	u32 cs;
	u32 eflags;
	u32 esp;
	u32 ss;
} exc_context_t;

#pragma pack(pop)


static inline int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	int prot = PROT_NONE;

	if (ctx->err & 1)
		prot |= PROT_READ;

	if (ctx->err & 2)
		prot |= PROT_WRITE;

	if (ctx->err & 4)
		prot |= PROT_USER;

	return prot;
}


static inline void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	u32 cr2;

	__asm__ volatile
	("movl %%cr2, %0"
	:"=r" (cr2)
	:
	:"eax");

	return (void *)cr2;
}


static inline ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->eip;
}


extern void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n);


extern int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *));


extern void _hal_exceptionsInit(void);

#endif

#endif
