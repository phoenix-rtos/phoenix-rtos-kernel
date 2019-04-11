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
	u32 eflags;
		
} spinlock_t;


static inline void hal_spinlockSet(spinlock_t *spinlock)
{
	int lock_val = 0;

	__asm__ volatile
	(" \
		pushf; \
		popl %0; \
		cli; \
	1: \
		xchgl %2, %1; \
		cmp $0, %1; \
		je 2f; \
		jmp 3f; \
	2:	pause; \
		jmp 1b; \
	3: "
	: "=r" (spinlock->eflags), "+r" (lock_val), "+m" (spinlock->lock)
	:
	: "memory");

	hal_cpuGetCycles((void *)&spinlock->b);	
}


static inline unsigned long hal_spinlockClearNoRestore(spinlock_t *spinlock)
{
	unsigned long flags;
	int unlock_val = 1;

	hal_cpuGetCycles((void *)&spinlock->e);
	
	/* Calculate maximum and minimum lock time */	
	if ((cycles_t)(spinlock->e - spinlock->b) > spinlock->dmax)
		spinlock->dmax = spinlock->e - spinlock->b;

	if (spinlock->e - spinlock->b < spinlock->dmin)
		spinlock->dmin = spinlock->e - spinlock->b;

	flags = spinlock->eflags;

	__asm__ volatile
	("	xchgl %1, %0"
	: "+r" (unlock_val), "+m" (spinlock->lock)
	:
	: "memory");

	return flags;
}


static inline void hal_spinlockClear(spinlock_t *spinlock)
{
	unsigned flags = hal_spinlockClearNoRestore(spinlock);

	__asm__ volatile
	(" \
		pushl %0; \
		popf"
	:
	: "r" (flags)
	: "memory");
}

extern void hal_spinlockCreate(spinlock_t *spinlock, const char *name);


extern void hal_spinlockDestroy(spinlock_t *spinlock);


extern void _hal_spinlockInit(void);


#endif

#endif
