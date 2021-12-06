/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../spinlock.h"
#include "../cpu.h"


struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlocks;


static void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock->lock = 1;

	spinlock->name = name;
	spinlock->dmin = (cycles_t)-1;
	spinlock->dmax = (cycles_t)0;

	if (spinlocks.first != NULL) {
		spinlocks.first->prev->next = spinlock;
		spinlock->prev = spinlocks.first->prev;
		spinlock->next = spinlocks.first;
		spinlocks.first->prev = spinlock;
	}
	else {
		spinlocks.first = spinlock;
		spinlock->next = spinlock;
		spinlock->prev = spinlock;
	}
	return;
}


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
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

	/* (MOD) inefficiency because of rdcycles instruction */
	/* hal_cpuGetCycles((void *)&spinlock->b); */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* (MOD) inefficiency because of rdcycles instruction */
	/* hal_cpuGetCycles((void *)&spinlock->e);*/

	/* Calculate maximum and minimum lock time */
	/*if ((cycles_t)(spinlock->e - spinlock->b) > spinlock->dmax)
		spinlock->dmax = spinlock->e - spinlock->b;

	if (spinlock->e - spinlock->b < spinlock->dmin)
		spinlock->dmin = spinlock->e - spinlock->b;*/

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


void hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlocks.spinlock, &sc);
	_hal_spinlockCreate(spinlock, name);
	hal_spinlockClear(&spinlocks.spinlock, &sc);
}


void hal_spinlockDestroy(spinlock_t *spinlock)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlocks.spinlock, &sc);

	if (spinlock->next == spinlock)
		spinlocks.first = NULL;
	else {
		spinlock->prev->next = spinlock->next;
		spinlock->next->prev = spinlock->prev;
	}
	spinlock->prev = spinlock->next = NULL;

	hal_spinlockClear(&spinlocks.spinlock, &sc);
	return;
}


__attribute__ ((section (".init"))) void _hal_spinlockInit(void)
{
	spinlocks.first = NULL;
	_hal_spinlockCreate(&spinlocks.spinlock, "spinlocks.spinlock");

	return;
}
