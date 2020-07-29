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

	u64 lock;
	u64 sstatus;
} spinlock_t;


typedef u64 spinlock_ctx_t;

static inline void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	__asm__ volatile
	(" \
		csrrc t0, sstatus, 2; \
		sd t0, %0; \
		mv t0, zero; \
1: \
		amoswap.w.aq t0, t0, %1; \
		beqz t0, 1b"
	:
	: "m" (spinlock->sstatus), "A" (spinlock->lock)
	: "t0", "memory");

	hal_cpuGetCycles((void *)&spinlock->b);
}


static inline void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_cpuGetCycles((void *)&spinlock->e);

	/* Calculate maximum and minimum lock time */
	if ((cycles_t)(spinlock->e - spinlock->b) > spinlock->dmax)
		spinlock->dmax = spinlock->e - spinlock->b;

	if (spinlock->e - spinlock->b < spinlock->dmin)
		spinlock->dmin = spinlock->e - spinlock->b;

	__asm__ volatile
	(" \
		li t1, 1; \
		amoswap.w.rl t1, t1, %0; \
		ld t0, %1; \
		csrw sstatus, t0"
	:
	: "A" (spinlock->lock), "m" (spinlock->sstatus)
	: "t0", "t1", "memory");

	return;
}


extern void hal_spinlockCreate(spinlock_t *spinlock, const char *name);


extern void hal_spinlockDestroy(spinlock_t *spinlock);


extern void _hal_spinlockInit(void);


#endif

#endif
