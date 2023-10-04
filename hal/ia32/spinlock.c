/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/spinlock.h"
#include "hal/cpu.h"

struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlocks;


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile (
		"pushf\n\t"
		"popl %%ebx\n\t"
		"cli\n"
		"1:\n\t"
		"xorl %%eax, %%eax\n\t"
		"xchgl %1, %%eax\n\t"
		"testl %%eax, %%eax\n\t"
		"jz 1b\n\t"
		"movl %%ebx, (%0)"
	:
	: "r" (sc), "m" (spinlock->lock)
	: "eax", "ebx", "memory");
	/* clang-format on */

	hal_cpuGetCycles((void *)&spinlock->b);
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_cpuGetCycles((void *)&spinlock->e);

	/* Calculate maximum and minimum lock time */
	if ((cycles_t)(spinlock->e - spinlock->b) > spinlock->dmax)
		spinlock->dmax = spinlock->e - spinlock->b;

	if (spinlock->e - spinlock->b < spinlock->dmin)
		spinlock->dmin = spinlock->e - spinlock->b;

	/* clang-format off */
	__asm__ volatile (
		"xorl %%eax, %%eax\n\t"
		"incl %%eax\n\t"
		"xchgl %0, %%eax\n\t"
		"movl %1, %%eax\n\t"
		"pushl %%eax\n\t"
		"popf"
	:
	: "m" (spinlock->lock), "r" (*sc)
	: "eax", "memory");
	/* clang-format on */

	return;
}


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
