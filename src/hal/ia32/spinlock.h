/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SPINLOCK_H_
#define _HAL_SPINLOCK_H_

#ifndef __ASSEMBLY__

#include "cpu.h"


typedef struct _spinlock_t {
	const char *name;
	cycles_t b;
	cycles_t e;
	cycles_t dmin;
	cycles_t dmax;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;

	u32 lock;
	u32 eflags[8];

} spinlock_t;


static inline void hal_spinlockSet(spinlock_t *spinlock)
{
	__asm__ volatile
	(" \
		pushf; \
		popl %%ebx; \
		cli; \
	1: \
		xorl %%eax, %%eax; \
		xchgl %1, %%eax; \
		cmp $0, %%eax; \
		jz 1b; \
		movl %%ebx, %0"
	:
	: "m" (spinlock->eflags[hal_cpuGetID()]), "m" (spinlock->lock)
	: "eax", "ebx", "memory");

	hal_cpuGetCycles((void *)&spinlock->b);
}


static inline void hal_spinlockClear(spinlock_t *spinlock)
{
	hal_cpuGetCycles((void *)&spinlock->e);

	/* Calculate maximum and minimum lock time */
	if ((cycles_t)(spinlock->e - spinlock->b) > spinlock->dmax)
		spinlock->dmax = spinlock->e - spinlock->b;

	if (spinlock->e - spinlock->b < spinlock->dmin)
		spinlock->dmin = spinlock->e - spinlock->b;

	__asm__ volatile
	(" \
		xorl %%eax, %%eax; \
		incl %%eax; \
		xchgl %0, %%eax; \
		movl %1, %%eax; \
		pushl %%eax; \
		popf"
	:
	: "m" (spinlock->lock), "m" (spinlock->eflags[hal_cpuGetID()])
	: "eax", "memory");

	return;
}


extern void hal_spinlockCreate(spinlock_t *spinlock, const char *name);


extern void hal_spinlockDestroy(spinlock_t *spinlock);


extern void _hal_spinlockInit(void);


#endif

#endif
