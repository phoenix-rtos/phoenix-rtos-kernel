/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2012, 2016, 2023 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/list.h"

struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlock_common;


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
}


static void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{

	spinlock->lock = 1;

	spinlock->name = name;
	spinlock->dmin = (cycles_t)-1;
	spinlock->dmax = (cycles_t)0;

	HAL_LIST_ADD(&spinlock_common.first, spinlock);
}


void hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlock_common.spinlock, &sc);
	_hal_spinlockCreate(spinlock, name);
	hal_spinlockClear(&spinlock_common.spinlock, &sc);
}


void hal_spinlockDestroy(spinlock_t *spinlock)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlock_common.spinlock, &sc);

	HAL_LIST_REMOVE(&spinlock_common.first, spinlock);

	hal_spinlockClear(&spinlock_common.spinlock, &sc);
}


__attribute__ ((section (".init"))) void _hal_spinlockInit(void)
{
	spinlock_common.first = NULL;
	_hal_spinlockCreate(&spinlock_common.spinlock, "spinlock_common.spinlock");
}
